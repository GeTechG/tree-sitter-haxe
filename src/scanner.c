#include "tree_sitter/parser.h"

#include <stdbool.h>
#include <stdint.h>

enum TokenType {
  INLINE_XML,
};

// Longest root markup name we compare against. Real tag names are short; a name
// longer than this simply never matches the root, which is harmless.
#define MAX_NAME 256

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

// Read a maximal xml_name (possibly empty) into `buf`, advancing the lexer.
// Returns the number of code points read (may exceed MAX_NAME, in which case
// the stored prefix is truncated and the name will not compare equal to root).
static int read_name(TSLexer *lexer, int32_t *buf, int *len) {
  int n = 0;
  if (is_name_start(lexer->lookahead)) {
    if (n < MAX_NAME) buf[n] = lexer->lookahead;
    n++;
    lexer->advance(lexer, false);
    while (is_name_char(lexer->lookahead)) {
      if (n < MAX_NAME) buf[n] = lexer->lookahead;
      n++;
      lexer->advance(lexer, false);
    }
  }
  *len = n;
  return n;
}

static bool name_equals(const int32_t *a, int alen, const int32_t *b, int blen) {
  if (alen != blen || alen > MAX_NAME) return false;
  for (int i = 0; i < alen; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
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
  if (!valid_symbols[INLINE_XML]) return false;

  while (is_whitespace(lexer->lookahead)) {
    lexer->advance(lexer, true);
  }
  if (lexer->lookahead != '<') return false;
  lexer->advance(lexer, false);

  // Only start markup when the root opening tag is plausible: a name-start char
  // or an empty-name fragment (`<>...</>`). This keeps stray `<` out of markup.
  if (!is_name_start(lexer->lookahead) && lexer->lookahead != '>') return false;

  int32_t root[MAX_NAME];
  int root_len = 0;
  read_name(lexer, root, &root_len);

  int depth = 0;
  bool in_open = root_len > 0; // a fragment (empty name) is never "in open"

  int32_t name[MAX_NAME];
  int name_len = 0;

  while (lexer->lookahead != 0) {
    int32_t c = lexer->lookahead;

    if (c == '<') {
      lexer->advance(lexer, false);
      if (lexer->lookahead == '/') {
        // Possible closing tag `</name>`.
        lexer->advance(lexer, false);
        read_name(lexer, name, &name_len);
        if (lexer->lookahead == '>') {
          lexer->advance(lexer, false);
          if (name_equals(name, name_len, root, root_len)) {
            if (depth == 0) {
              lexer->mark_end(lexer);
              lexer->result_symbol = INLINE_XML;
              return true;
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
      read_name(lexer, name, &name_len);
      if (name_equals(name, name_len, root, root_len)) {
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
          return true;
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
  return false;
}
