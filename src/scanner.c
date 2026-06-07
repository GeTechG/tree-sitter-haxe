#include "tree_sitter/parser.h"

#include <stdbool.h>
#include <stdint.h>

enum TokenType {
  INLINE_XML,
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

static bool is_tag_start(int32_t c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static bool is_whitespace(int32_t c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

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
  if (!is_tag_start(lexer->lookahead)) return false;

  unsigned depth = 1;
  unsigned brace_depth = 0;
  int tag_kind = 1;
  int32_t quote = 0;
  bool in_tag = true;

  while (lexer->lookahead != 0) {
    int32_t c = lexer->lookahead;

    if (quote != 0) {
      lexer->advance(lexer, false);
      if (c == '\\' && lexer->lookahead != 0) {
        lexer->advance(lexer, false);
      } else if (c == quote) {
        quote = 0;
      }
      continue;
    }

    if (in_tag) {
      if (c == '"' || c == '\'') {
        quote = c;
        lexer->advance(lexer, false);
        continue;
      }
      if (c == '{') {
        brace_depth++;
        lexer->advance(lexer, false);
        continue;
      }
      if (c == '}' && brace_depth > 0) {
        brace_depth--;
        lexer->advance(lexer, false);
        continue;
      }
      if (brace_depth == 0 && c == '/') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == '>') {
          lexer->advance(lexer, false);
          if (tag_kind == 1) depth--;
          if (depth == 0) {
            lexer->mark_end(lexer);
            lexer->result_symbol = INLINE_XML;
            return true;
          }
          in_tag = false;
        }
        continue;
      }
      if (brace_depth == 0 && c == '>') {
        lexer->advance(lexer, false);
        if (tag_kind == -1) depth--;
        if (depth == 0) {
          lexer->mark_end(lexer);
          lexer->result_symbol = INLINE_XML;
          return true;
        }
        in_tag = false;
        continue;
      }
      lexer->advance(lexer, false);
      continue;
    }

    if (c != '<') {
      lexer->advance(lexer, false);
      continue;
    }

    lexer->advance(lexer, false);
    if (lexer->lookahead == '/') {
      tag_kind = -1;
      lexer->advance(lexer, false);
    } else if (lexer->lookahead == '!' || lexer->lookahead == '?') {
      tag_kind = 0;
      lexer->advance(lexer, false);
    } else if (is_tag_start(lexer->lookahead)) {
      tag_kind = 1;
      depth++;
    } else {
      return false;
    }
    in_tag = true;
  }

  return false;
}
