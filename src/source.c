#include "source.h"

#include <_stdio.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

/*
 * ファイル全文をアリーナに読む。
 */
bool source_load(source_t *src, const char *filename, arena_t *a, diag_t *d){
    char msg[DIAG_MSG_MAX];

    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        (void)snprintf(msg, sizeof msg, "cannot open '%s': %s", filename, strerror(errno));
        diag_error_noloc(d, msg);
        return false;
    }

    /* サイズを取得 */
    if (fseek(f, 0, SEEK_END) != 0) {
        (void)snprintf(msg, sizeof msg, "cannot open '%s': %s", filename, strerror(errno));
        diag_error_noloc(d, msg);
        goto fail;
    }
    long size= ftell(f);

    if (size < 0) {
        (void)snprintf(msg, sizeof msg, "cannot open '%s': %s", filename, strerror(errno));
        diag_error_noloc(d, msg);
        goto fail;
    }

    rewind(f);

    char * buf = arena_alloc(a, (size_t)size + 1);
    size_t n = fread(buf, 1, (size_t)size, f);
    if (n != (size_t)size) {
        if (ferror(f)) {
            (void)snprintf(msg, sizeof msg, "cannot read '%s': %s", filename, strerror(errno));
        } else {
            (void)snprintf(msg, sizeof msg, "cannot read '%s'", filename);
        }
        diag_error_noloc(d, msg);
        goto fail;
    }

    buf[size] = '\0';

    (void)fclose(f);
    src->filename = filename;
    src->text = buf;
    src->len = (size_t)size;
    src->pos = 0;
    src->line = 1;
    src->col = 1;
    return true;

fail:
    (void)fclose(f);
    return false;
}

/* 改行判定 */
static bool is_newline(int c) {
    return c == '\r' || c == '\n';
}

/*
 * 進めずに次の１文字を見る。終端ならSOURCE_EOF。
 */
int source_peek(const source_t *src) {
    if (src->pos >= src->len) { return SOURCE_EOF;}
    int c = (int)(unsigned char)src->text[src->pos];
    return is_newline(c) ? '\n' : c;
}

/*
 * 進めずに２文字目を見る。最長一致の判定に使う。
 */
int source_peek2(const source_t *src) {
    if (src->pos + 1 >= src->len) {return SOURCE_EOF;}
    return (int)(unsigned char)src->text[src->pos + 1];
}

/*
 * １文字進めて、その文字を返す
 * pos == len でも text[len] は NUL なので読んで安全
 */
int source_get(source_t *src) {
    if (src->pos >= src->len) { return SOURCE_EOF;} /* POSを進めない */

    int c = (int)(unsigned char)src->text[src->pos];    /* pos == lenのことがある */
    src->pos++;

    if (is_newline(c)) {
        int n = (int)(unsigned char)src->text[src->pos];
        /* /r/nと/n/rの場合を見る */
        if (is_newline(n) && n != c) {
            src->pos++;
        }
        src->line++;
        src->col = 1;
        return '\n';
    } else {
        src->col++;
        return c;
    }
}
