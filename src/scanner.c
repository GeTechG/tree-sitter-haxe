#include "tree_sitter/parser.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

enum TokenType {
  INLINE_XML,
  FLOAT_TRAILING_DOT,
};

void *tree_sitter_haxe_external_scanner_create(void) {
  return NULL;
}

void tree_sitter_haxe_external_scanner_destroy(void *payload) {
  (void)payload;
}

unsigned tree_sitter_haxe_external_scanner_serialize(void *payload, char *buffer) {
  (void)payload;
  (void)buffer;
  return 0;
}

void tree_sitter_haxe_external_scanner_deserialize(
  void *payload,
  const char *buffer,
  unsigned length
) {
  (void)payload;
  (void)buffer;
  (void)length;
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
bool tree_sitter_haxe_external_scanner_scan(
  void *payload,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  (void)payload;
  if (!valid_symbols[FLOAT_TRAILING_DOT] && !valid_symbols[INLINE_XML]) {
    return false;
  }

  // Leading whitespace is shared by both external tokens.
  while (is_whitespace(lexer->lookahead)) {
    lexer->advance(lexer, true);
  }

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

  // Only start markup when the root opening tag is plausible: a name-start char
  // or an empty-name fragment (`<>...</>`). This keeps stray `<` out of markup.
  if (!is_name_start(lexer->lookahead) && lexer->lookahead != '>') return false;

  NameBuf root = {NULL, 0, 0};
  bool result = false;
  if (!read_root_name(lexer, &root)) goto done;

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
              lexer->mark_end(lexer);
              lexer->result_symbol = INLINE_XML;
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
          lexer->mark_end(lexer);
          lexer->result_symbol = INLINE_XML;
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
