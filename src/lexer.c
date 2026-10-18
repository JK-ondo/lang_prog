#include "lexer.h"

static bool is_letter(int c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool is_digit(int c) {
  return c >= '0' && c <= '9';
}

static bool is_name_char(int c) {
  return is_letter(c) || is_digit(c);
}

/*
 * 初期化
 */
void lexer_init(lexer_t *lx, source_t *src, arena_t *arena, diag_t *diag) {
  lx->src = src;
  lx->arena = arena;
  lx->diag = diag;
}

/*
 * 識別子を読み取る
 * 事前条件：トークンの先頭が英字である
 */
static token_t lex_name(lexer_t *lx, token_t tok) {
  const char *start = lx->src->text + lx->src->pos;
  while (is_name_char(source_peek(lx->src))) {
    (void)source_get(lx->src);
  }
  tok.text = start;
  tok.len = (size_t)(lx->src->text + lx->src->pos - start);
  tok.kind = token_keyword_lookup(start, tok.len);
  return tok;
}

/*
 * 数値を読み取る
 * 事前条件：トークンの先頭が数字である
 */
static token_t lex_number(lexer_t *lx, token_t tok) {
  const char *start = lx->src->text + lx->src->pos;
  unsigned value = 0;
  bool over = false;
  while (is_digit(source_peek(lx->src))) {
    int c = source_get(lx->src);
    if (!over) {
      value = value * 10u + (unsigned)(c - '0');
      if (value > MPPL_NUMBER_MAX) {
        over = true;
      }
    }
  }
  tok.kind = TOK_NUMBER;
  tok.text = start;
  tok.len = (size_t)(lx->src->text + lx->src->pos - start);
  tok.value.number = over ? 0u : value;
  if (over) {
    char msg[DIAG_MSG_MAX];
    (void)snprintf(msg, sizeof msg, "number out of range: %.*s", (int)tok.len, tok.text);
    diag_error(lx->diag, tok.line, tok.col, msg);
  }
  return tok;
}

/*
 * 次のトークンを１つ返す。
 */
token_t lexer_next(lexer_t *lx) {
  /* 空白、タブ、改行を読み飛ばす */
  for (;;) {
    int c = source_peek(lx->src);
    if (c == ' ' || c == '\t' || c == '\n') {
      (void)source_get(lx->src);
      continue;
    }
    if (c == '{') {
      /* {---}注釈を読み飛ばす */
      (void)source_get(lx->src);
      for (;;) {
        int k = source_get(lx->src);
        if (k == SOURCE_EOF || k == '}') {
          break;
        }
      }
      continue;
    }
    if (c == '/' && source_peek2(lx->src) == '*') {
      /* 注釈を読み飛ばす */
      (void)source_get(lx->src);
      (void)source_get(lx->src);
      for (;;) {
        int k = source_get(lx->src);
        if (k == SOURCE_EOF) {
          break;
        }
        if ((k == '*' && source_peek(lx->src) == '/')) {
          (void)source_get(lx->src);
          break;
        }
      }
      continue;
    }
    break; /* 分離子でないので、トークンの先頭 */
  }

  /* トークンの先頭の位置を控える */
  token_t tok = {0};
  tok.line = lx->src->line;
  tok.col = lx->src->col;

  int c = source_peek(lx->src);
  if (c == SOURCE_EOF) {
    return tok;
  }
  if (is_letter(c)) {
    return lex_name(lx, tok);
  }
  if (is_digit(c)) {
    return lex_number(lx, tok);
  }

  return tok;
}
