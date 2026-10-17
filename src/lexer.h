#ifndef MPPLC_LEXER_H
#define MPPLC_LEXER_H

#include "arena.h"
#include "diag.h"
#include "source.h"
#include "token.h"

typedef struct {
  source_t *src;  /* 所有しない。呼び出し側がもつ */
  arena_t *arena; /* ''を含む文字列の複製先。所有しない */
  diag_t *diag;   /* 字句エラーの報告先。所有しない */
} lexer_t;

/*
 * 初期化
 * 事前条件：srcはsource_load済み。arenaとdiagは有効。３つともlexerより長生き。
 * 事後条件：３つのポインタを記憶するのみ。sourceの位置は触らない。
 */
void lexer_init(lexer_t *lx, source_t *src, arena_t *arena, diag_t *diag);

/*
 * 次のトークンを１つ返す。
 * 事前条件：lexer_init済み。
 * 事後条件：分離子（空白・タブ・改行・注釈）を飛ばしてから１トークン読む。
 *           入力の終わりではkind == TOK_EOFを返し続ける（sourceが進まない）。
 *           字句エラーの時はdiagに報告し、問題の文字を飛ばして次の正当なトークンを返す
 *           （TOK_ERRORはない）
 */
token_t lexer_next(lexer_t *lx);

#endif /* MPPLC_LEXER_H */
