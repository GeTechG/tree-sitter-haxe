#include "tree_sitter/parser.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum TokenType {
  INLINE_XML,
  FLOAT_TRAILING_DOT,
  IF,
  CUT_IF,
  CUT_CONDITION,
  CONDITIONAL_INACTIVE,
  CUT_END,
  END_MARK,
  STRING_CONTENT,
};

// Open `#if`s deeper than this are taken as whole-node conditionals.
#define MAX_DEPTH 255

// A conditional whose branch has an unpaired bracket cuts a construct: the
// grammar cannot give it as a node. Its directives are extras instead, and
// `cut` remembers, for each open `#if`, which kind its `#else` and `#end`
// belong to.
//
// The `#end` of a whole-node conditional is the grammar's token, and the
// scanner's state is kept only with a token of its own. So before such an
// `#end` it gives an empty END_MARK, and `marked_end` tells that `#end` from
// the next one when the scanner is asked again at the same place.
typedef struct {
  uint16_t depth;
  uint16_t marked_end;    // `ends_ahead` at the marked `#end`, 0: none
  bool condition_pending; // after the `#if` of a cut conditional
  bool cut[MAX_DEPTH];
} Scanner;

void *tree_sitter_haxe_external_scanner_create(void) {
  return calloc(1, sizeof(Scanner));
}

void tree_sitter_haxe_external_scanner_destroy(void *payload) {
  free(payload);
}

unsigned tree_sitter_haxe_external_scanner_serialize(void *payload, char *buffer) {
  Scanner *scanner = (Scanner *)payload;
  unsigned stored = scanner->depth < MAX_DEPTH ? scanner->depth : MAX_DEPTH;
  buffer[0] = (char)(scanner->depth & 0xFF);
  buffer[1] = (char)(scanner->depth >> 8);
  buffer[2] = (char)(scanner->marked_end & 0xFF);
  buffer[3] = (char)(scanner->marked_end >> 8);
  buffer[4] = (char)scanner->condition_pending;
  memcpy(buffer + 5, scanner->cut, stored);
  return 5 + stored;
}

void tree_sitter_haxe_external_scanner_deserialize(
  void *payload,
  const char *buffer,
  unsigned length
) {
  Scanner *scanner = (Scanner *)payload;
  memset(scanner, 0, sizeof(Scanner));
  if (length < 5) return;
  scanner->depth = (uint16_t)((uint8_t)buffer[0] | ((uint8_t)buffer[1] << 8));
  scanner->marked_end =
    (uint16_t)((uint8_t)buffer[2] | ((uint8_t)buffer[3] << 8));
  scanner->condition_pending = buffer[4];
  memcpy(scanner->cut, buffer + 5, length - 5);
}

// Mirror the Haxe lexer's `xml_name_start_char`
// (HaxeFoundation/haxe src/syntax/lexer.ml): ASCII letters, '_', plus '$' and
// ':' for JSX-style markup, and the XML 1.0 Unicode name-start ranges.
static bool is_name_start(int32_t c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' ||
         c == '$' || c == ':' ||
         (c >= 0xC0 && c <= 0xD6) || (c >= 0xD8 && c <= 0xF6) ||
         (c >= 0xF8 && c <= 0x2FF) || (c >= 0x370 && c <= 0x37D) ||
         (c >= 0x37F && c <= 0x1FFF) || (c >= 0x200C && c <= 0x200D) ||
         (c >= 0x2070 && c <= 0x218F) || (c >= 0x2C00 && c <= 0x2FEF) ||
         (c >= 0x3001 && c <= 0xD7FF) || (c >= 0xF900 && c <= 0xFDCF) ||
         (c >= 0xFDF0 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0xEFFFF);
}

// Mirror the Haxe lexer's `xml_name_char`: name-start chars plus '-', '.',
// digits, and the XML 1.0 Unicode name-continuation ranges.
static bool is_name_char(int32_t c) {
  return is_name_start(c) || c == '-' || c == '.' || (c >= '0' && c <= '9') ||
         c == 0xB7 || (c >= 0x0300 && c <= 0x036F) ||
         (c >= 0x203F && c <= 0x2040);
}

static bool is_whitespace(int32_t c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

// Haxe identifier-start character (src/syntax/lexer.ml `ident`): ASCII letter or
// underscore. Used to decline a trailing-dot float when a field access follows.
static bool is_haxe_ident_start(int32_t c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

// Scan a trailing-dot float `N.` (e.g. `0.`, `1_000.`): a decimal integer
// followed by a single `.` that does NOT begin an interval `...`, a fractional
// part, or a field access. Returns true (and marks the token) only on a real
// trailing-dot float; otherwise leaves tokenization to the internal lexer.
static bool scan_float_trailing_dot(TSLexer *lexer) {
  if (lexer->lookahead < '0' || lexer->lookahead > '9') return false;
  while ((lexer->lookahead >= '0' && lexer->lookahead <= '9') ||
         lexer->lookahead == '_') {
    lexer->advance(lexer, false);
  }
  if (lexer->lookahead != '.') return false;
  lexer->advance(lexer, false);
  int32_t after = lexer->lookahead;
  if (after == '.' || (after >= '0' && after <= '9') || is_haxe_ident_start(after)) {
    return false;
  }
  lexer->mark_end(lexer);
  lexer->result_symbol = FLOAT_TRAILING_DOT;
  return true;
}

// Growable buffer of code points for the root tag name. The root name can be
// arbitrarily long (Haxe imposes no length limit), so we do not cap it.
typedef struct {
  int32_t *data;
  size_t len;
  size_t cap;
} NameBuf;

static bool name_push(NameBuf *b, int32_t c) {
  if (b->len == b->cap) {
    size_t ncap = b->cap ? b->cap * 2 : 16;
    int32_t *nd = (int32_t *)realloc(b->data, ncap * sizeof(int32_t));
    if (!nd) return false;
    b->data = nd;
    b->cap = ncap;
  }
  b->data[b->len++] = c;
  return true;
}

// Read the maximal xml_name at the current position into `out`, advancing the
// lexer. A name may be empty (fragments). Returns false only on allocation
// failure.
static bool read_root_name(TSLexer *lexer, NameBuf *out) {
  if (is_name_start(lexer->lookahead)) {
    if (!name_push(out, lexer->lookahead)) return false;
    lexer->advance(lexer, false);
    while (is_name_char(lexer->lookahead)) {
      if (!name_push(out, lexer->lookahead)) return false;
      lexer->advance(lexer, false);
    }
  }
  return true;
}

// Read the maximal xml_name at the current position, advancing the lexer, while
// streaming-comparing it against `root`. Returns true iff the scanned name
// equals the root name exactly (no buffering of the candidate needed, so the
// candidate length is unbounded).
static bool read_name_matches_root(TSLexer *lexer, const NameBuf *root) {
  size_t idx = 0;
  bool match = true;
  if (is_name_start(lexer->lookahead)) {
    int32_t c = lexer->lookahead;
    if (idx >= root->len || c != root->data[idx]) match = false; else idx++;
    lexer->advance(lexer, false);
    while (is_name_char(lexer->lookahead)) {
      c = lexer->lookahead;
      if (idx >= root->len || c != root->data[idx]) match = false; else idx++;
      lexer->advance(lexer, false);
    }
  }
  return match && idx == root->len;
}

// Bounded inline-XML markup, following HaxeFoundation/haxe src/syntax/lexer.ml
// `lex_xml`/`not_xml`: depth tracks only repetitions of the *root* tag name,
// there is intentionally no string/quote balancing, and `/>` self-closes only
// while still inside the root opening tag (fragments cannot self-close).
// Skips the markup after its `<`; false when none starts or ends here.
static bool skip_markup(TSLexer *lexer) {
  // Only start markup when the root opening tag is plausible: a name-start char
  // or an empty-name fragment (`<>...</>`). This keeps stray `<` out of markup.
  if (!is_name_start(lexer->lookahead) && lexer->lookahead != '>') return false;

  NameBuf root = {NULL, 0, 0};
  bool result = false;
  if (!read_root_name(lexer, &root)) goto done;

  // A comma after the name is a type-argument list (`E<T, ~//>`), never markup:
  // tag attributes are not comma-separated.
  while (is_whitespace(lexer->lookahead)) {
    lexer->advance(lexer, false);
  }
  if (lexer->lookahead == ',') goto done;

  int depth = 0;
  bool in_open = root.len > 0; // a fragment (empty name) is never "in open"

  while (lexer->lookahead != 0) {
    int32_t c = lexer->lookahead;

    if (c == '<') {
      lexer->advance(lexer, false);
      if (lexer->lookahead == '/') {
        // Possible closing tag `</name>`.
        lexer->advance(lexer, false);
        bool matches = read_name_matches_root(lexer, &root);
        if (lexer->lookahead == '>') {
          lexer->advance(lexer, false);
          if (matches) {
            if (depth == 0) {
              result = true;
              goto done;
            }
            depth--;
          } else {
            in_open = false;
          }
        }
        // A `</name` not followed by `>` is consumed as content.
        continue;
      }
      // Possible opening tag `<name` (name may be empty).
      if (read_name_matches_root(lexer, &root)) {
        depth++;
        in_open = true;
      } else {
        in_open = false;
      }
      continue;
    }

    if (c == '/') {
      lexer->advance(lexer, false);
      if (lexer->lookahead == '>') {
        lexer->advance(lexer, false);
        if (in_open) depth--;
        if (depth < 0) {
          result = true;
          goto done;
        }
        in_open = false;
      }
      // A lone `/` is content.
      continue;
    }

    // Lone `>`, quotes, braces, and any other character are plain content
    // (Haxe performs no quote/brace balancing inside markup).
    lexer->advance(lexer, false);
  }

  // Reached EOF without closing the root tag: unterminated markup.
done:
  free(root.data);
  return result;
}

static bool is_ident_char(int32_t c) {
  return is_haxe_ident_start(c) || (c >= '0' && c <= '9');
}

// Skip to past the closing `quote`; the opening one is already consumed.
static void skip_quoted(TSLexer *lexer, int32_t quote) {
  while (lexer->lookahead != 0 && lexer->lookahead != quote) {
    if (lexer->lookahead == '\\') lexer->advance(lexer, false);
    lexer->advance(lexer, false);
  }
  lexer->advance(lexer, false);
}

// Skip a string, a regexp or a comment starting here, whose brackets and `#`
// are text. Returns false, consuming nothing, when none starts here.
// ponytail: a quote inside the `${}` of a '-string ends it early, and one in
// inline markup opens a string; follow those tokens if a real file needs it.
static bool skip_text(TSLexer *lexer) {
  int32_t c = lexer->lookahead;
  if (c == '"' || c == '\'') {
    lexer->advance(lexer, false);
    skip_quoted(lexer, c);
    return true;
  }
  if (c == '~') {
    lexer->advance(lexer, false);
    if (lexer->lookahead == '/') {
      lexer->advance(lexer, false);
      skip_quoted(lexer, '/');
    }
    return true;
  }
  if (c == '/') {
    lexer->advance(lexer, false);
    if (lexer->lookahead == '/') {
      while (lexer->lookahead != 0 && lexer->lookahead != '\n') {
        lexer->advance(lexer, false);
      }
    } else if (lexer->lookahead == '*') {
      lexer->advance(lexer, false);
      int32_t previous = 0;
      while (lexer->lookahead != 0 &&
             !(previous == '*' && lexer->lookahead == '/')) {
        previous = lexer->lookahead;
        lexer->advance(lexer, false);
      }
      lexer->advance(lexer, false);
    }
    return true;
  }
  return false;
}

typedef enum { D_OTHER, D_IF, D_ELSE, D_END } Directive;

// Read the directive at `#`. `#elseif` and `#else` are one kind: both end a
// branch.
static Directive read_directive(TSLexer *lexer) {
  char word[8] = {0};
  unsigned length = 0;
  lexer->advance(lexer, false);
  while (is_ident_char(lexer->lookahead)) {
    if (length < sizeof(word) - 1) word[length] = (char)lexer->lookahead;
    length++;
    lexer->advance(lexer, false);
  }
  if (length >= sizeof(word)) return D_OTHER;
  if (strcmp(word, "if") == 0) return D_IF;
  if (strcmp(word, "else") == 0 || strcmp(word, "elseif") == 0) return D_ELSE;
  if (strcmp(word, "end") == 0) return D_END;
  return D_OTHER;
}

// Skip one operand of a condition: a name, a literal, a parenthesised
// expression, any of them under `!`.
static bool skip_condition_operand(TSLexer *lexer) {
  for (;;) {
    while (is_whitespace(lexer->lookahead)) lexer->advance(lexer, false);
    if (lexer->lookahead != '!') break;
    lexer->advance(lexer, false);
  }
  int32_t c = lexer->lookahead;
  if (c == '(') {
    int depth = 0;
    do {
      if (lexer->lookahead == 0) return false;
      if (skip_text(lexer)) continue;
      if (lexer->lookahead == '(') depth++;
      if (lexer->lookahead == ')') depth--;
      lexer->advance(lexer, false);
    } while (depth > 0);
    return true;
  }
  if (c == '"' || c == '\'') return skip_text(lexer);
  if (!is_ident_char(c)) return false;
  while (is_ident_char(lexer->lookahead) || lexer->lookahead == '.') {
    lexer->advance(lexer, false);
  }
  return true;
}

// Skip the condition of an `#if`, marking its end if asked: operands joined by `&&`,
// `||` and comparisons, as `compile_condition` takes them without parentheses.
static bool skip_condition(TSLexer *lexer, bool mark) {
  for (bool first = true;; first = false) {
    // An operator with no operand after it was the branch.
    if (!skip_condition_operand(lexer)) return !first;
    if (mark) lexer->mark_end(lexer);
    while (is_whitespace(lexer->lookahead)) lexer->advance(lexer, false);
    int32_t c = lexer->lookahead;
    if (c != '&' && c != '|' && c != '=' && c != '!' && c != '<' && c != '>') {
      return true;
    }
    lexer->advance(lexer, false);
    if (c == '&' || c == '|' || c == '=' || c == '!') {
      // `&&`, `||`, `==`, `!=`: anything else is the branch already.
      if (lexer->lookahead != (c == '!' ? '=' : c)) return true;
      lexer->advance(lexer, false);
    } else if (lexer->lookahead == '=') {
      lexer->advance(lexer, false);
    }
  }
}

// From after `#if` to its `#end`: whether a branch has an unpaired bracket.
// A conditional nested in a branch counts by its first branch, the one that
// is parsed if it is cut itself; were it not cut, no branch would matter.
static bool is_cut(TSLexer *lexer) {
  if (!skip_condition(lexer, false)) return false;
  bool cut = false;
  int32_t previous = 0; // the last character of code
  int brackets = 0;
  unsigned nested = 0;  // open `#if`s inside this one
  unsigned ignored = 0; // the nested level whose later branch we are in
  while (lexer->lookahead != 0) {
    int32_t c = lexer->lookahead;
    if (is_whitespace(c)) {
      lexer->advance(lexer, false);
      continue;
    }
    // After an operand `<` compares or opens type parameters; anywhere else
    // it opens markup, whose brackets are text.
    // ponytail: `return <a>(</a>` is still read as code; needs the keyword.
    if (c == '<' && !is_ident_char(previous) && previous != ')' &&
        previous != ']' && previous != '}' && previous != '"' &&
        previous != '<') {
      lexer->advance(lexer, false);
      skip_markup(lexer);
      previous = '>';
      continue;
    }
    // A string ends an operand, a comment changes nothing.
    if (c != '/') previous = c == '\'' || c == '~' ? '"' : c;
    if (skip_text(lexer)) continue;
    if (c == '#') {
      switch (read_directive(lexer)) {
        case D_IF:
          nested++;
          if (!skip_condition(lexer, false)) return false;
          break;
        case D_ELSE:
          if (nested == 0) {
            if (brackets != 0) cut = true;
            brackets = 0;
          } else if (ignored == 0) {
            ignored = nested;
          }
          break;
        case D_END:
          if (nested == 0) return cut || brackets != 0;
          if (ignored == nested) ignored = 0;
          nested--;
          break;
        case D_OTHER:
          break;
      }
      continue;
    }
    if (ignored == 0) {
      if (c == '(' || c == '[' || c == '{') brackets++;
      if (c == ')' || c == ']' || c == '}') {
        // A closing bracket with nothing open in the branch.
        if (brackets == 0) cut = true; else brackets--;
      }
    }
    lexer->advance(lexer, false);
  }
  // No `#end`: leave the error to the grammar.
  return false;
}

// The branches of a cut conditional after the first, up to its `#end`.
static bool scan_inactive(TSLexer *lexer) {
  unsigned nested = 0;
  lexer->mark_end(lexer);
  while (lexer->lookahead != 0) {
    if (is_whitespace(lexer->lookahead)) {
      lexer->advance(lexer, false);
      continue;
    }
    if (lexer->lookahead == '#') {
      Directive directive = read_directive(lexer);
      if (directive == D_END && nested-- == 0) {
        lexer->result_symbol = CONDITIONAL_INACTIVE;
        return true;
      }
      if (directive == D_IF) nested++;
    } else if (!skip_text(lexer)) {
      lexer->advance(lexer, false);
    }
    lexer->mark_end(lexer);
  }
  return false;
}

// The number of `#end`s from the one just read to the next `#if`: no two
// `#end`s without a scanner token between them have the same.
static uint16_t ends_ahead(TSLexer *lexer) {
  uint16_t ends = 1;
  while (lexer->lookahead != 0) {
    if (lexer->lookahead == '#') {
      Directive directive = read_directive(lexer);
      if (directive == D_IF) break;
      if (directive == D_END && ends < UINT16_MAX) ends++;
    } else if (!skip_text(lexer)) {
      lexer->advance(lexer, false);
    }
  }
  return ends;
}

static bool scan_directive(Scanner *scanner, TSLexer *lexer) {
  bool in_cut = scanner->depth > 0 && scanner->depth <= MAX_DEPTH &&
                scanner->cut[scanner->depth - 1];
  lexer->mark_end(lexer); // END_MARK is empty
  switch (read_directive(lexer)) {
    case D_IF: {
      lexer->mark_end(lexer);
      bool cut = scanner->depth < MAX_DEPTH && is_cut(lexer);
      if (scanner->depth < MAX_DEPTH) scanner->cut[scanner->depth] = cut;
      if (scanner->depth < UINT16_MAX) scanner->depth++;
      scanner->condition_pending = cut;
      scanner->marked_end = 0;
      lexer->result_symbol = cut ? CUT_IF : IF;
      return true;
    }
    case D_ELSE:
      // In a whole-node conditional `#else` is the grammar's own token.
      if (!in_cut || !scan_inactive(lexer)) return false;
      scanner->marked_end = 0;
      return true;
    case D_END: {
      if (in_cut) {
        lexer->mark_end(lexer);
        scanner->depth--;
        scanner->marked_end = 0;
        lexer->result_symbol = CUT_END;
        return true;
      }
      uint16_t ends = ends_ahead(lexer);
      // Marked already: now it is the grammar's to read.
      if (scanner->marked_end == ends) return false;
      if (scanner->depth > 0) scanner->depth--;
      scanner->marked_end = ends;
      lexer->result_symbol = END_MARK;
      return true;
    }
    case D_OTHER:
      return false;
  }
  return false;
}

bool tree_sitter_haxe_external_scanner_scan(
  void *payload,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  Scanner *scanner = (Scanner *)payload;
  // Inside a string `#if` is text. In error recovery every symbol is valid.
  if (valid_symbols[STRING_CONTENT] && !valid_symbols[CUT_CONDITION]) {
    return false;
  }

  // Leading whitespace is shared by all external tokens.
  while (is_whitespace(lexer->lookahead)) {
    lexer->advance(lexer, true);
  }

  if (scanner->condition_pending) {
    scanner->condition_pending = false;
    if (!skip_condition(lexer, true)) return false;
    lexer->result_symbol = CUT_CONDITION;
    return true;
  }

  if (lexer->lookahead == '#') return scan_directive(scanner, lexer);

  if (valid_symbols[FLOAT_TRAILING_DOT]) {
    if (lexer->lookahead >= '0' && lexer->lookahead <= '9') {
      // A digit can only begin the trailing-dot float here, never inline XML;
      // decline (resetting the lexer) when it is an ordinary number.
      return scan_float_trailing_dot(lexer);
    }
  }

  if (!valid_symbols[INLINE_XML]) return false;
  if (lexer->lookahead != '<') return false;
  lexer->advance(lexer, false);

  if (!skip_markup(lexer)) return false;
  lexer->mark_end(lexer);
  lexer->result_symbol = INLINE_XML;
  return true;
}
