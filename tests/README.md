# テスト

テストは「入力ソースを与えて、出力と終了コードを期待値と比べる」形を基本にする。実装の内部
構造に触らないので、段階が進んで内部を書き換えても、前の段階のテストがそのまま残る。

```sh
mise run test                          # 全件
ctest --test-dir build -R stage1       # 段階1だけ
ctest --test-dir build -R lexer/number # 名前で絞る
```

## ケースの置き方

`tests/cases/<段階>/<名前>.src` を置くと、CMake が `case/<段階>/<名前>` という名前のテスト
として自動登録する。ビルドし直せば拾われる（`mise run build`）。

同じ場所に置く期待値ファイル:

| ファイル       | 意味                                                 | 無いとき         |
| -------------- | ---------------------------------------------------- | ---------------- |
| `<名前>.out`   | 期待する標準出力（完全一致）                         | 検査しない       |
| `<名前>.err`   | 期待する標準エラー（完全一致）                       | 検査しない       |
| `<名前>.code`  | 期待する終了コード                                   | `0` を期待       |
| `<名前>.args`  | `.src` より前に渡す追加の引数（空白区切り、1 行）    | 引数なし         |

例 — 字句解析の段階で `--dump-tokens` を付けて走らせ、トークン列を比べる:

```
tests/cases/stage1-lexer/number.src    →  1 + 23
tests/cases/stage1-lexer/number.args   →  --dump-tokens
tests/cases/stage1-lexer/number.out    →  期待するトークン列
```

エラーを期待するケース:

```
tests/cases/stage1-lexer/bad-char.src   →  1 @ 2
tests/cases/stage1-lexer/bad-char.code  →  1
tests/cases/stage1-lexer/bad-char.err   →  期待するエラーメッセージ
```

## 期待値は先に書く

`.out` は「仕様からこうなるはず」と考えて手で書く。動かしてから出力を写すと、実装が何であ
れテストは通るので、何も確かめていないことになる。

出力が長くて写経が辛いときだけ `UPDATE=1` を使えるが、**生成された中身を必ず自分で読んで
から**コミットする。

```sh
UPDATE=1 tests/run_case.sh build/lppc tests/cases/stage1-lexer/number.src
```
