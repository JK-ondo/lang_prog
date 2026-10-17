/*
 * コンパイラの入口。
 *
 * MPPLC_LAST_IMPLEMENTED_PHASE 1 つで未実装を判定する。
 * 段階1（字句解析）から実装していく。
 * 使い方とオプションは docs/stages/ の各段階の仕様に合わせて増やす。
 */

#include <stdio.h>
#include <string.h>

#include "arena.h"
#include "diag.h"
#include "lexer.h"
#include "options.h"
#include "source.h"
#include "token.h"

/*
 * 要求フェーズ->未実装エラーに出す名前
 * 外部仕様 §2.1の表
 */

static const char *phase_name(phase_t p) {
  switch (p) {
  case PHASE_LEX:
    return "lexical analysis";
  case PHASE_PARSE:
    return "parsing";
  case PHASE_SEMANTIC:
    return "semantic analysis";
  case PHASE_CODEGEN:
    return "code generation";
  }
}

/*
 * 入力ファイルの拡張子が.mplでなければ警告
 * 外部仕様 §3.1, §4.3
 */
static void warn_bad_extension(diag_t *d, const char *path) {
  const char *ext = strrchr(path, '.');
  if (ext == NULL || strcmp(ext, ".mpl") != 0) {
    /* 拡張子が正しくない */
    char msg[DIAG_MSG_MAX];
    (void)snprintf(msg, sizeof msg, "input file '%s' does not end in '.mpl'", path);
    diag_warning_noloc(d, "bad-file-extension", msg);
  }
}

static void dump_token(FILE *out, const token_t *tok) {
  (void)fprintf(out, "%d:%d\t%s", tok->line, tok->col, token_kind_symbol(tok->kind));
  if (tok->kind == TOK_EOF) {
    (void)fprintf(out, "\n");
    return;
  }
  if (tok->kind == TOK_STRING) {
    (void)fprintf(out, "\t%zu '", tok->value.str.len);
    for (size_t i = 0; i < tok->value.str.len; i++) {
      char ch = tok->value.str.s[i];
      switch (ch) {
      case '\'':
        (void)fputs("\\'", out);
        break;
      case '\t':
        (void)fputs("\\t", out);
        break;
      case '\\':
        (void)fputs("\\\\", out);
        break;
      default:
        (void)fputc(ch, out);
        break;
      }
    }
    (void)fputs("\n", out);
  } else {
    (void)fprintf(out, "\t%.*s\n", (int)tok->len, tok->text);
  }
}

static void run_lex(const options_t *opt, lexer_t *lx) {
  for (;;) {
    token_t tok = lexer_next(lx);
    if (opt->dump_tokens) {
      dump_token(stdout, &tok);
    }

    if (tok.kind == TOK_EOF) {
      break;
    }
  }
}

int main(int argc, char **argv) {
  diag_config_t cfg = {
      .filename = NULL, .no_warnings = false, .warnings_are_errors = false, .max_errors = 0};
  diag_t d;
  diag_init(&d, &cfg, stderr);

  options_t opt;

  /* argvを解析 */
  switch (options_parse(&opt, argc, argv, &d)) {
  case OPTIONS_FINISHED:
    return 0;
  case OPTIONS_ERROR:
    return 1;
  case OPTIONS_OK:
    break;
  }

  cfg.filename = opt.input;
  cfg.no_warnings = opt.no_warnings;
  cfg.warnings_are_errors = opt.warnings_are_errors;
  cfg.max_errors = opt.max_errors;
  diag_init(&d, &cfg, stderr);

  warn_bad_extension(&d, opt.input);

  if (opt.requested > MPPLC_LAST_IMPLEMENTED_PHASE) {
    /* 未実装の判定 */
    char msg[DIAG_MSG_MAX];
    (void)snprintf(msg, sizeof msg, "%s is not implemented yet", phase_name(opt.requested));
    diag_error_noloc(&d, msg);
    return 1;
  }

  arena_t *a = arena_create();
  source_t src;
  if (!source_load(&src, opt.input, a, &d)) {
    arena_destroy(a);
    return 1;
  }

  lexer_t lx;
  lexer_init(&lx, &src, a, &d);
  run_lex(&opt, &lx);

  arena_destroy(a);

  return diag_error_count(&d) > 0 ? 1 : 0;
}
