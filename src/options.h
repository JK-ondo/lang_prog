#ifndef MPPLC_OPTIONS_H
#define MPPLC_OPTIONS_H

#include <stdbool.h>

#include "diag.h"

typedef enum { PHASE_LEX, PHASE_PARSE, PHASE_SEMANTIC, PHASE_CODEGEN } phase_t;

#define MPPLC_LAST_IMPLEMENTED_PHASE PHASE_LEX

typedef struct {
  const char *input;        /* input-file NULLなら無指定 */
  phase_t requested;        /* 課題の要求フェーズ */
  bool dump_tokens;         /* --dump-tokens */
  bool no_warnings;         /* -w */
  bool warnings_are_errors; /* -Werror */
  unsigned max_errors;      /* -fmax-errors=n 0なら無制限 */
} options_t;

typedef enum {
  OPTIONS_OK,       /* 続行する */
  OPTIONS_FINISHED, /* --help / --version を出した。終了コード０ */
  OPTIONS_ERROR     /* 診断ずみ。終了コード１ */
} options_status_t;

/*
 * 事前条件：opt != NULL && d != NULL
 *           dはdiag_init済み
 * 事後条件：OPTIONS_OKを返した時だけ*optが有効
 *           OPTIONS_ERRORの時は診断を出し終えている(呼び出し側は何も出さない)
 */
options_status_t options_parse(options_t *opt, int argc, char **argv, diag_t *d);

#endif /* MPPLC_OPTIONS_H */
