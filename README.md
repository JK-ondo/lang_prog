# lpp — 4段階でコンパイラを実装

授業課題として、コンパイラを 4 段階に分けて実装する。1 本のツリーを段階ごとに育て、各段
階の完成時点を git タグと GitHub リリースで固定する。

| 段階 | 内容           | タグ | 仕様書                                     |
| ---- | -------------- | ---- | ------------------------------------------ |
| 1    | 字句解析       | `v1` | [docs/stages/stage1-lexer.md]              |
| 2    | 構文解析 (AST) | `v2` | [docs/stages/stage2-parser.md]             |
| 3    | 意味解析       | `v3` | [docs/stages/stage3-semantic.md]           |
| 4    | コード生成     | `v4` | [docs/stages/stage4-codegen.md]            |

実装言語は **C17**。ビルドは CMake + Ninja。

## about

```sh
mise tasks          # 使えるタスク一覧
mise run build      # ビルド
mise run test       # テスト
mise run verify     # 検証一式（提出前にこれを緑にする）
```

生成物は `build/mpplc`。

## directory layout

```
src/            コンパイラ本体（C）
tests/cases/    入力と期待出力の対（実装言語に依存しない）
tests/run_case.sh  ケースを1件走らせるハーネス
docs/           仕様・段階ごとの設計・決定記録・授業メモ
examples/       サンプル入力プログラム
```

ドキュメントは [docs/README.md]。

[docs/README.md]: docs/README.md
[docs/stages/stage1-lexer.md]: docs/stages/stage1-lexer.md
[docs/stages/stage2-parser.md]: docs/stages/stage2-parser.md
[docs/stages/stage3-semantic.md]: docs/stages/stage3-semantic.md
[docs/stages/stage4-codegen.md]: docs/stages/stage4-codegen.md
