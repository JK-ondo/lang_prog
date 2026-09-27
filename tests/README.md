# テスト

テストは「入力ソースを与えて、出力と終了コードを期待値と比べる」形を基本にする。実装の内部
構造に触らないので、段階が進んで内部を書き換えても、前の段階のテストがそのまま残る。

```sh
mise run test                          # 全件
ctest --test-dir build -R stage1       # 段階1だけ
ctest --test-dir build -R lexer/number # 名前で絞る
```

## ケースの置き方

`tests/cases/<段階>/<名前>.src` か `<名前>.argv` を置くと、CMake が `case/<段階>/<名前>` という
名前のテストとして自動登録する。ビルドし直せば拾われる（`mise run build`）。

同じ場所に置くファイル:

| ファイル       | 意味                                                        | 無いとき     |
| -------------- | ----------------------------------------------------------- | ------------ |
| `<名前>.src`   | 入力となる MPPL ソース                                      | —            |
| `<名前>.args`  | `.src` より前に渡す追加の引数（空白区切り）                 | 引数なし     |
| `<名前>.argv`  | **引数列の全体。`.src` を自動で渡さない**（`.args` と排他） | `.args` を使う |
| `<名前>.out`   | 期待する標準出力（完全一致）                                | 検査しない   |
| `<名前>.err`   | 期待する標準エラー（完全一致）                              | 検査しない   |
| `<名前>.code`  | 期待する終了コード                                          | `0` を期待   |

コンパイラはケースのディレクトリで起動するので、引数に書くパスはそのディレクトリからの相対に
なる。出力に現れるファイル名が環境に依存しないのはこのため。

### `.args` — ふつうのケース

`.src` を入力として渡す。字句解析の段階で `--dump-tokens` を付けて走らせる例:

```
tests/cases/stage1-lexer/number.src    →  1 + 23
tests/cases/stage1-lexer/number.args   →  --dump-tokens
tests/cases/stage1-lexer/number.out    →  期待するトークン列
```

実際に走るのは `mpplc --dump-tokens number.src`。

エラーを期待するケース:

```
tests/cases/stage1-lexer/bad-char.src   →  1 @ 2
tests/cases/stage1-lexer/bad-char.code  →  1
tests/cases/stage1-lexer/bad-char.err   →  期待するエラーメッセージ
```

### `.argv` — 引数列そのものを検査するケース

`.argv` があると `.src` を自動で渡さない。**入力ファイルを渡さない場合や、2 つ渡す場合**を
検査できる。`.src` は不要で、`.argv` だけでテストが登録される。

入力ファイルが 0 個（`.argv` は空ファイル）:

```
tests/cases/cli/no-input.argv   →  （空）
tests/cases/cli/no-input.err    →  mpplc: error: no input file
tests/cases/cli/no-input.code   →  1
```

入力ファイルが 2 つ。`.src` を 2 つ置くとテストが 2 件登録されてしまうので、入力には `.mpl`
などの別の拡張子を使う（glob の対象は `.src` と `.argv` だけ）:

```
tests/cases/cli/multi-input.argv  →  one.mpl two.mpl
tests/cases/cli/one.mpl           →  program p; begin end.
tests/cases/cli/two.mpl           →  program q; begin end.
tests/cases/cli/multi-input.err   →  mpplc: error: multiple input files ...
tests/cases/cli/multi-input.code  →  1
```

不明なオプション。`.argv` の中で自分の `.src` を参照してもよい:

```
tests/cases/cli/unknown-option.argv  →  -Xfoo unknown-option.src
tests/cases/cli/unknown-option.src   →  program p; begin end.
tests/cases/cli/unknown-option.code  →  1
```

## 期待値は先に書く

`.out` は「仕様からこうなるはず」と考えて手で書く。動かしてから出力を写すと、実装が何であ
れテストは通るので、何も確かめていないことになる。

出力が長くて写経が辛いときだけ `UPDATE=1` を使えるが、**生成された中身を必ず自分で読んで
から**コミットする。

`run_case.sh` に渡すのは**拡張子を取り除いた基底パス**であることに注意（`.src` ではない）。

```sh
UPDATE=1 tests/run_case.sh build/mpplc tests/cases/stage1-lexer/number
```
