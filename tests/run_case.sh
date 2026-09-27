#!/bin/sh
# ケーステストを1件走らせる。
#   run_case.sh <コンパイラのパス> <ケースの .src>
#
# .src の隣に置いたファイルが期待値になる（詳細は tests/README.md）:
#   <名前>.out   期待する標準出力（完全一致。無ければ標準出力を検査しない）
#   <名前>.err   期待する標準エラー（完全一致。無ければ検査しない）
#   <名前>.code  期待する終了コード（無ければ 0）
#   <名前>.args  .src より前に渡す追加の引数（空白区切り、1 行）
#
# コンパイラはケースのディレクトリで、入力を basename で渡して起動する。出力に現れるファイル名
# が `foo.src` になり、期待値が環境のパスに依存しない。
#
# UPDATE=1 を付けて走らせると .out / .err を実際の出力で上書きする。期待値は仕様から先に書く
# もので、これは「手で確認した出力を書き写す手間」を省くためだけにある。中身を読まずに上書き
# したテストは何も保証しない。

set -eu

if [ "$#" -ne 2 ]; then
  echo "usage: run_case.sh <compiler> <case.src>" >&2
  exit 2
fi

# cd する前に、渡されたパスを絶対パスに直す。
to_abs() {
  case $1 in
  /*) printf '%s\n' "$1" ;;
  *) printf '%s/%s\n' "$(pwd)" "$1" ;;
  esac
}

compiler=$(to_abs "$1")
src=$(to_abs "$2")
base=${src%.src}
dir=$(dirname "$src")
name=$(basename "$base")

if [ ! -x "$compiler" ]; then
  echo "コンパイラが実行できない: $compiler" >&2
  exit 2
fi
if [ ! -f "$src" ]; then
  echo "ケースが無い: $src" >&2
  exit 2
fi

expected_code=0
if [ -f "$base.code" ]; then
  expected_code=$(cat "$base.code")
fi

args=""
if [ -f "$base.args" ]; then
  args=$(cat "$base.args")
fi

# 作業場所は cwd の下に置く。ctest から呼ばれるときの cwd はビルドツリーなので汚れない。
# 別段階の同名ケースが衝突しないよう、ケースのパスから一意な鍵を作る。
key=$(printf '%s' "$base" | tr -c 'A-Za-z0-9._-' '_')
work=$(pwd)/.case-work/$key
rm -rf "$work"
mkdir -p "$work"
trap 'rm -rf "$work"' EXIT INT TERM

set +e
# shellcheck disable=SC2086  # .args は意図的に単語分割する
(cd "$dir" && "$compiler" $args "$name.src") >"$work/out" 2>"$work/err"
actual_code=$?
set -e

if [ "${UPDATE:-0}" = "1" ]; then
  cp "$work/out" "$base.out"
  if [ -f "$base.err" ]; then
    cp "$work/err" "$base.err"
  fi
  if [ "$actual_code" -ne 0 ]; then
    printf '%s\n' "$actual_code" >"$base.code"
  fi
  echo "更新: $name（期待値の中身を必ず自分で確認すること）"
  exit 0
fi

status=0
checked_output=0

if [ "$actual_code" -ne "$expected_code" ]; then
  echo "[$name] 終了コードが違う: 期待 $expected_code / 実際 $actual_code"
  status=1
fi

if [ -f "$base.out" ]; then
  checked_output=1
  if ! diff -u "$base.out" "$work/out" >"$work/diff.out"; then
    echo "[$name] 標準出力が違う (- 期待 / + 実際):"
    cat "$work/diff.out"
    status=1
  fi
fi

if [ -f "$base.err" ]; then
  checked_output=1
  if ! diff -u "$base.err" "$work/err" >"$work/diff.err"; then
    echo "[$name] 標準エラーが違う (- 期待 / + 実際):"
    cat "$work/diff.err"
    status=1
  fi
fi

# 期待値ファイルが無いケースが落ちたときは、手掛かりとして実際の出力を見せる。
if [ "$status" -ne 0 ] && [ "$checked_output" -eq 0 ]; then
  echo "[$name] 実際の標準出力:"
  cat "$work/out"
  echo "[$name] 実際の標準エラー:"
  cat "$work/err"
fi

exit "$status"
