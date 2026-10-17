#include "lexer.h"

/*
 * 初期化
 */
void lexer_init(lexer_t *lx, source_t *src, arena_t *arena, diag_t *diag) {
  lx->src = src;
  lx->arena = arena;
  lx->diag = diag;
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

  /*
   * ここでTOKEN分類をする。
   */
  return tok;
}
