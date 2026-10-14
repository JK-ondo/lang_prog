#include "arena.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARENA_BLOCK_MIN (64u * 1024u)

struct block {
    struct block * next;
    char * data;            /* mallocで取った領域。mallocの戻りはmax_align_t境界に揃ってる */
    size_t cap;             /* dataの大きさ */
    size_t used;            /* 使用量 */
};

struct arena {
    struct block * head;    /* 先頭が切り出し中のブロック */
};

/* memoryが不足した場合の例外として出力経路 */
static _Noreturn void die(void) {
    (void)fputs("mpplc: out of memory\n", stderr);
    exit(1);
}

/*
 * sizeバイトを返す。max_align_t境界に整列している。
 * 失敗したら落ちるので。呼び出し側はNULLチェックしない。
 */
void * arena_alloc (arena_t * a, size_t size) {
    /* 必要データサイズを整列境界に切り上げて丸める */
    if (size > SIZE_MAX - _Alignof(max_align_t)) {die();}
    size_t need = (size + _Alignof(max_align_t) - 1) / _Alignof(max_align_t) * _Alignof(max_align_t);
    /* headに収まるかを見る */
    if (a->head == NULL || a->head->used + need  > a->head->cap) {
        /* headに収まらない。新しいブロックを作ってheadの前に繋ぐ。 */
        struct block *b = malloc(sizeof *b);
        if (b == NULL) {die();}
        b->cap = (ARENA_BLOCK_MIN > need) ? ARENA_BLOCK_MIN : need;
        b->data = malloc(b->cap);
        if (b->data == NULL) {die();}
        b->used = 0;
        b->next = a->head;
        a->head = b;
    }
    char * p = a->head->data + a->head->used;
    a->head->used += need;
    return p;
}

/* sの先頭nバイトを複製して、NULを付けて返す */
char * arena_strndup(arena_t *a, const char *s, size_t n){
    char * p = arena_alloc(a, n+1);
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* アリーナ全体を返す。ここから取得した領域は以後全て無効。 */
void arena_destroy(arena_t * a){
    if (a == NULL) return;
    struct block * p;
    while (a->head != NULL) {
        p = a->head;
        a->head = p->next;
        free(p->data);
        free(p);
    }
    free(a);
}

/* アリーナを作る。失敗すると落ちるので、戻り値は必ず非NULL */
arena_t * arena_create(void){
    arena_t *a = malloc(sizeof *a);
    if (a == NULL) {die();}
    a->head = NULL;
    return a;
}
