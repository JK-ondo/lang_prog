#ifndef MPPLC_SOURCE_H
#define MPPLC_SOURCE_H

#include <stdbool.h>
#include <stddef.h>

#include "arena.h"
#include "diag.h"

/* 終端を表す値。0~255のどの文字とも重ならないので-1 */
#define SOURCE_EOF (-1)

typedef struct {
    const char * filename;  /* 診断に出す名前。所有しない */
    const char * text;      /* アリーナ上の全文。NUL終端済み */
    size_t len;             /* NULを含まない長さ */
    size_t pos;             /* 次に読む位置。0<=pos<=len */
    int line;               /* 次に読む文字の行。１起点 */
    int col;                /* 次に読む文字の桁。１起点 */
} source_t;

/*
 * ファイル全文をアリーナに読む。
 * 事前条件：srcは未初期化で良い。aとdは有効。
 * 事後条件：成功ならtextが全文を指し、pos=0,line=1,col=1でtrueを返す。
 *           失敗ならdiag_error_nolocを呼んでfalseを返す（srcの中身は不定）。
 */
bool source_load(source_t *src, const char *filename, arena_t *a, diag_t *d);

/*
 * 進めずに次の１文字を見る。終端ならSOURCE_EOF。
 * 事前条件：source_load済み。
 * 事後条件：状態を変えない。
 */
int source_peek(const source_t *src);

/*
 * 進めずに２文字目を見る。最長一致の判定に使う。
 * 事前条件：source_load済み。
 * 事後条件：状態を変えない。
 */
int source_peek2(const source_t * src);

/*
 * １文字進めて、その文字を返す
 * 事前条件：source_load済み。
 * 事後条件：posが進み,lineとcolが更新される。
 *           改行は種類を問わずに'\n'を返す。
 *           終端ではposを進めずにSOURCE_EOFを返し続ける。
 */
int source_get(source_t *src);

#endif /* MPPLC_SOURCE_H */
