#ifndef MPPLC_DIAG_H
#define MPPLC_DIAG_H

#include <stdbool.h>
#include <stdio.h>

/*
 * 診断メッセージの最大長。
 * 呼ぶ側はこの大きさの配列に snprintf で組んでから渡す。
 * 切り捨てるのは呼ぶ側で、diag はバッファを持たない。
 * snprintf は必ず NUL 終端するので、切れても安全。
 * 診断が切れてもコンパイル結果は変わらない。
 */
#define DIAG_MSG_MAX 512

/*
 * 診断に出すプログラム名。
 * 位置を持たない診断の行頭に付く。
 */
#define DIAG_PROG_NAME "mpplc"

typedef struct {
  const char *filename;     /* 診断に出すファイル名。所有しない */
  bool no_warnings;         /* -w */
  bool warnings_are_errors; /* -Werror */
  unsigned max_errors;      /* -fmax-errors=n。0 は無制限 */
} diag_config_t;

/*
 * 診断の状態。
 * 数える場所は errors の 1 つだけで、警告用の数え場所は持たない。
 * errors は「エラーとして扱われた診断の数」で、-Werror で昇格した
 * 警告もここに入る。main はこの値だけで、次のフェーズに進むかと
 * 終了コードを 1 にするかを決める。
 */
typedef struct {
  diag_config_t cfg;   /* diag_init で複製する */
  FILE *out;           /* 出力先。所有しない */
  unsigned errors;     /* エラーとして扱われた診断の数 */
  bool limit_reported; /* 打ち切り行を出したか */
} diag_t;

/* 初期化
 * 事前条件：out は NULL でない、cfg->filename は d より長生き。
 * 事後条件：cfg を複製する。filename はポインタを写すだけで文字列は
 *           複製しないので、長生きが事前条件になる。
 *           d->errors を 0、d->limit_reported を false にする。
 */
void diag_init(diag_t *d, const diag_config_t *cfg, FILE *out);

/* ソース中の位置を持つ診断 */

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
void diag_error(diag_t *d, int line, int col, const char *msg);

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
void diag_warning(diag_t *d, int line, int col, const char *opt, const char *msg);

/* 位置を持たない診断（コマンドラインやファイル入出力について） */

/* 位置なしでエラーを 1 件報告する。
 * 事前条件：diag_init 済み。
 * 事後条件：行頭が DIAG_PROG_NAME になり位置を出さないことだけが
 *           diag_error と違う。d->errors の増減と打ち切りの扱いは
 *           diag_error と同じ。
 */
void diag_error_noloc(diag_t *d, const char *msg);

/* 位置なしで警告を 1 件報告する。
 * 事前条件：diag_init 済み、opt は NULL でない。
 * 事後条件：行頭が DIAG_PROG_NAME になり位置を出さないことだけが
 *           diag_warning と違う。4 段の分岐は diag_warning と同じ。
 */
void diag_warning_noloc(diag_t *d, const char *opt, const char *msg);

/* いま数えているエラーの数を返す。
 * 事前条件：diag_init 済み。
 * 事後条件：状態は変えない。d->errors をそのまま返す。
 *           -Werror で昇格した警告は d->errors に入っているので、
 *           この値に含まれる。
 */
unsigned diag_error_count(const diag_t *d);

/* -fmax-errors の上限に達したか。フェーズはこれを見て早めに抜ける。
 * 事前条件：diag_init 済み。
 * 事後条件：状態は変えない。
 *           d->cfg.max_errors != 0 && d->errors >= d->cfg.max_errors
 *           前半を落とすと、max_errors が 0（無制限）のときに
 *           1 件目を出す前から打ち切り済みになる。
 */
bool diag_should_stop(const diag_t *d);

#endif /* MPPLC_DIAG_H */
