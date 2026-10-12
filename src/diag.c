/*
 * 診断出力
 *
 * オプションに合わせて処理を変化させる
 * stderrに書くのはこのファイルのみ
 */

#include "diag.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

typedef enum {
  /* 3つの診断種別 */
  DIAG_ERROR,
  DIAG_WARNING,
  DIAG_ERROR_PROMOTED,
  /* 種別の数。表の大きさの検査に使う */
  DIAG_KIND_COUNT
} diag_kind_t;

static const struct {
  const char *word;   /* error か warning */
  const char *prefix; /* カッコ内の接頭辞。NULLならカッコを出さない */
} kinds[] = {
    [DIAG_ERROR] = {"error", NULL},
    [DIAG_WARNING] = {"warning", "-W"},
    [DIAG_ERROR_PROMOTED] = {"error", "-Werror="},
};

static_assert(sizeof kinds / sizeof kinds[0] == DIAG_KIND_COUNT,
              "診断種別を足したらkindにも行を足す");

static void emit(const diag_t *d, diag_kind_t kind, bool has_loc, int line, int col,
                 const char *opt, const char *msg) {
  if (has_loc) {
    (void)fprintf(d->out, "%s:%d:%d: %s: %s", d->cfg.filename, line, col, kinds[kind].word, msg);
  } else {
    (void)fprintf(d->out, DIAG_PROG_NAME ": %s: %s", kinds[kind].word, msg);
  }
  if (kinds[kind].prefix != NULL) {
    (void)fprintf(d->out, " [%s%s]", kinds[kind].prefix, opt);
  }
  (void)fprintf(d->out, "\n");
}

static void report_limit(diag_t *d) {
  if (diag_should_stop(d) && !d->limit_reported) {
    (void)fprintf(d->out, "compilation terminated due to -fmax-errors=%u.\n", d->cfg.max_errors);
    d->limit_reported = true;
  }
}

/* 初期化
 * 事前条件：out は NULL でない、cfg->filename は d より長生き。
 * 事後条件：cfg を複製する。filename はポインタを写すだけで文字列は
 *           複製しないので、長生きが事前条件になる。
 *           d->errors を 0、d->limit_reported を false にする。
 */
void diag_init(diag_t *d, const diag_config_t *cfg, FILE *out) {
  d->cfg = *cfg;             /* cfgを複製 */
  d->out = out;              /* 診断出力先を記憶 */
  d->errors = 0;             /* d->errorsを初期化 */
  d->limit_reported = false; /* 打ち切り行を既に出したか */
}

/* いま数えているエラーの数を返す。
 * 事前条件：diag_init 済み。
 * 事後条件：状態は変えない。d->errors をそのまま返す。
 *           -Werror で昇格した警告は d->errors に入っているので、
 *           この値に含まれる。
 */
unsigned diag_error_count(const diag_t *d) {
  return d->errors;
}

/* -fmax-errors の上限に達したか。フェーズはこれを見て早めに抜ける。
 * 事前条件：diag_init 済み。
 * 事後条件：状態は変えない。
 *           d->cfg.max_errors != 0 && d->errors >= d->cfg.max_errors
 *           前半を落とすと、max_errors が 0（無制限）のときに
 *           1 件目を出す前から打ち切り済みになる。
 */
bool diag_should_stop(const diag_t *d) {
  return d->cfg.max_errors != 0 && d->errors >= d->cfg.max_errors;
}

/* エラーを 1 件報告する。
 * 事前条件：diag_init 済み。
 * 事後条件：次の順で行う。「数える」と「出す」は別で、数えるほうは
 *           打ち切りの後も止まらない。
 *             1. d->errors を必ず 1 増やす。-w の影響を受けない
 *                （警告は消せてもエラーは消せない）
 *             2. limit_reported が false なら本文を出す
 *             3. 増やした後に diag_should_stop(d) が true で
 *                limit_reported が false なら、打ち切り行を出して
 *                limit_reported を true にする（文言は外部仕様 §5.1）
 *           出力をやめても数え続けるのは、終了コードが d->errors だけで
 *           決まるため。黙った分だけ増えても 1 のままで変わらない。
 */
void diag_error(diag_t *d, int line, int col, const char *msg) {
  d->errors++;
  if (!d->limit_reported) {
    emit(d, DIAG_ERROR, true, line, col, NULL, msg);
  }
  report_limit(d);
}

/* 位置なしでエラーを 1 件報告する。
 * 事前条件：diag_init 済み。
 * 事後条件：行頭が DIAG_PROG_NAME になり位置を出さないことだけが
 *           diag_error と違う。d->errors の増減と打ち切りの扱いは
 *           diag_error と同じ。
 */
void diag_error_noloc(diag_t *d, const char *msg) {
  d->errors++;
  if (!d->limit_reported) {
    emit(d, DIAG_ERROR, false, 0, 0, NULL, msg);
  }
  report_limit(d);
}

/* 警告を 1 件報告する。
 * 事前条件：diag_init 済み、opt は NULL でない。
 * 事後条件：次の順で行う。順番そのものが仕様。
 *             1. -w なら何も出さず d->errors も変えない。-w は -Werror
 *                より強く、消すと言われたものを昇格させない
 *             2. -Werror なら d->errors を 1 増やす。そうでなければ
 *                d->errors は変えない
 *             3. limit_reported が false なら出す。-Werror なら
 *                error: として [-Werror=<名前>] を付け、そうでなければ
 *                warning: として [-W<名前>] を付ける
 *             4. 2 で増やした結果 diag_should_stop(d) が true になり
 *                limit_reported が false なら、打ち切り行を出して
 *                limit_reported を true にする。昇格した警告も打ち切りを
 *                起こせる
 */
void diag_warning(diag_t *d, int line, int col, const char *opt, const char *msg) {
  if (d->cfg.no_warnings) {
    return;
  }
  diag_kind_t kind = d->cfg.warnings_are_errors ? DIAG_ERROR_PROMOTED : DIAG_WARNING;
  if (kind == DIAG_ERROR_PROMOTED) {
    d->errors++;
  }
  if (!d->limit_reported) {
    emit(d, kind, true, line, col, opt, msg);
  }
  report_limit(d);
}

/* 位置なしで警告を 1 件報告する。
 * 事前条件：diag_init 済み、opt は NULL でない。
 * 事後条件：行頭が DIAG_PROG_NAME になり位置を出さないことだけが
 *           diag_warning と違う。4 段の分岐は diag_warning と同じ。
 */
void diag_warning_noloc(diag_t *d, const char *opt, const char *msg) {
  if (d->cfg.no_warnings) {
    return;
  }
  diag_kind_t kind = d->cfg.warnings_are_errors ? DIAG_ERROR_PROMOTED : DIAG_WARNING;
  if (kind == DIAG_ERROR_PROMOTED) {
    d->errors++;
  }
  if (!d->limit_reported) {
    emit(d, kind, false, 0, 0, opt, msg);
  }
  report_limit(d);
}
