#!/bin/sh
# tests/cases.tsv と tests/cases/ のファイルが食い違っていないかを検査する。
#
# 「表が正」という規約（tests/README.md）を機械で守るためのもの。表に無いケースが置かれて
# いたり、表にあるのにファイルが無かったりすると落ちる。期待値の中身は見ない（それは
# run_case.sh の仕事）。
#
# while ループをパイプで回さないこと。パイプの右側は副シェルになり、中で立てた status が
# 外に伝わらない（失敗しても終了コード 0 になる）。

set -u

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tsv=$root/cases.tsv
cases=$root/cases
status=0
seen=""
tab=$(printf '\t')

note() {
  echo "$1" >&2
  status=1
}

if [ ! -f "$tsv" ]; then
  echo "$tsv が無い" >&2
  exit 2
fi

# 1. 列数と名前の重複
if ! awk -F'\t' 'NF != 10 { printf "cases.tsv:%d: 列が %d 個（10 でない。タブが空白に化けていないか）\n", NR, NF; bad = 1 }
                 END { exit bad ? 1 : 0 }' "$tsv" >&2; then
  status=1
fi
dups=$(awk -F'\t' 'NR > 1 && $1 != "×" { key = $2 "/" $4; if (seen[key]++ == 1) print key }' "$tsv")
if [ -n "$dups" ]; then
  note "ケース名が重複: $dups"
fi

# 2. 表の各行に対応するファイルがあるか
lineno=0
while IFS="$tab" read -r state dir kind name input args code checks reason rest; do
  lineno=$((lineno + 1))
  # 見出し行、作らないケース（×）、未着手（-）はファイルが無くてよいので飛ばす
  if [ "$lineno" -eq 1 ] || [ "$state" = "×" ] || [ "$state" = "-" ]; then
    continue
  fi
  base=$cases/$dir/$name
  seen="$seen$dir/$name
"

  case $args in
  argv:*)
    if [ ! -f "$base.argv" ]; then note "$dir/$name: .argv が無い（引数欄が argv: で始まっている）"; fi
    if [ -f "$base.src" ]; then note "$dir/$name: .argv と .src が両方ある（排他）"; fi
    ;;
  *)
    if [ ! -f "$base.src" ]; then note "$dir/$name: .src が無い"; fi
    if [ ! -f "$base.args" ]; then note "$dir/$name: .args が無い（引数欄が ${args}）"; fi
    ;;
  esac

  # 終了コード: 0 なら .code を置かなくてよい
  actual_code=0
  if [ -f "$base.code" ]; then actual_code=$(cat "$base.code"); fi
  if [ "$actual_code" != "$code" ]; then
    note "$dir/$name: 終了コードが表と違う（表 $code / ファイル ${actual_code}）"
  fi

  # 検査欄と期待値ファイルの対応
  case $checks in
  *out空*)
    if [ ! -f "$base.out" ] || [ -s "$base.out" ]; then note "$dir/$name: .out が無いか空でない（表は out空）"; fi
    ;;
  *out*)
    if [ ! -s "$base.out" ]; then note "$dir/$name: .out が無いか空（表は out）"; fi
    ;;
  esac
  case $checks in
  *err空*)
    if [ ! -f "$base.err" ] || [ -s "$base.err" ]; then note "$dir/$name: .err が無いか空でない（表は err空）"; fi
    ;;
  *err*)
    if [ ! -s "$base.err" ]; then note "$dir/$name: .err が無いか空（表は err）"; fi
    ;;
  esac
done <"$tsv"

# 3. 表に無いケースが置かれていないか
for f in "$cases"/*/*.src "$cases"/*/*.argv; do
  if [ ! -e "$f" ]; then continue; fi
  rel=${f#"$cases"/}
  key=${rel%.*}
  if ! printf '%s' "$seen" | grep -qx "$key"; then
    note "$key: ファイルがあるのに cases.tsv に行が無い"
  fi
done

if [ "$status" -eq 0 ]; then
  echo "cases.tsv と tests/cases/ は一致している"
fi
exit "$status"
