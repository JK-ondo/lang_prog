/* コンパイラの入口。
 *
 * いまは引数の受け取りだけで、変換は何も行わない。段階1（字句解析）から実装していく。
 * 使い方とオプションは docs/stages/ の各段階の仕様に合わせて増やす。 */

#include <stdio.h>
#include <string.h>

/* 出力に argv[0] を使わない。ビルド場所によって変わる文字列が出ると、テストの期待値が
 * 環境に依存してしまう。 */
static const char *const prog = "mpplc";

static void usage(FILE *out) {
  fprintf(out, "usage: %s [options] <source>\n", prog);
  fprintf(out, "  -h, --help  この使い方を表示する\n");
}

int main(int argc, char **argv) {
  if (argc < 2) {
    usage(stderr);
    return 2;
  }
  if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
    usage(stdout);
    return 0;
  }

  fprintf(stderr, "%s: まだ何も実装されていない (入力: %s)\n", prog, argv[argc - 1]);
  return 1;
}
