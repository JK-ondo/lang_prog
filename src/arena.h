#ifndef MPPLC_ARENA_H
#define MPPLC_ARENA_H

#include <stddef.h>

/* 実装はarena.cに隠す */
typedef struct arena arena_t;

/* アリーナを作る。失敗すると落ちるので、戻り値は必ず非NULL */
arena_t * arena_create(void);

/* sizeバイトを返す。max_align_t境界に整列している。
 * 失敗したら落ちるので。呼び出し側はNULLチェックしない。
 */
void * arena_alloc(arena_t * a, size_t size);

/* sの先頭nバイトを複製して、NULを付けて返す */
char * arena_strndup(arena_t * a, const char * s, size_t n);

/* アリーナ全体を返す。ここから取得した領域は以後全て無効。 */
void arena_destroy(arena_t * a);

#endif /* MPPLC_ARENA_H */
