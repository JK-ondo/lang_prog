# 開発の回し方

## 前提

ツールチェーンは mise で固定されている。この 2 つが必要:

| 用途           | ツール                                    | 状態                     |
| -------------- | ----------------------------------------- | ------------------------ |
| コンパイル     | Apple clang (`/usr/bin/clang`)            | OS 同梱                  |
| ビルド生成     | cmake (mise) + ninja                      | 導入済み                 |
| 整形           | `clang-format`                            | **未導入**（下記参照）   |
| 静的解析       | `clang --analyze`                         | Apple clang 同梱         |

`clang-format` が入るまで `mise run verify` は `verify:fmt` で落ちる。導入は
`mise use -g aqua:llvm/clang-format` 相当を**自分で**実行する（このリポジトリの設定では
ツールを勝手に入れない方針）。整形検査を後回しにするなら `mise run verify:build`
`mise run verify:test` を個別に走らせる。

## 日常のコマンド

```sh
mise tasks                   # 一覧
mise run build               # Debug ビルド（ASan/UBSan つき）
mise run test                # ケーステスト
mise run run -- examples/x.src   # 手で動かす
mise run clean               # build/ を消す
```

`src/` に `.c` を足したら `mise run build` をもう一度走らせるだけでよい（CMakeLists の編集は
不要）。

## 提出前

```sh
mise run verify
```

内訳:

| leaf           | 中身                                                   |
| -------------- | ------------------------------------------------------ |
| `verify:fmt`   | `clang-format --dry-run --Werror`                      |
| `verify:lint`  | `clang --analyze`（未初期化・NULL 参照・リークを見る） |
| `verify:build` | `-Werror` 付きの Release ビルド                        |
| `verify:test`  | `ctest`（ASan/UBSan 有効のバイナリで）                 |

C には別の型検査器がないので、`-Werror` 付きビルドがその役目を負う。

## サニタイザ

Debug ビルドには AddressSanitizer と UndefinedBehaviorSanitizer が入る。コンパイラは
ポインタとバッファを大量に扱うので、「なぜか動く」実装をここで落とす。切りたいときは
`cmake -S . -B build -DLPP_SANITIZE=OFF`。

## 進め方の順番

1. その段階の仕様（`docs/stages/stageN-*.md`）の空欄を埋める。授業の課題文が出所。
2. 仕様から期待値を考えて `tests/cases/stageN-*/` にケースを書く。**実装より先**。
3. 落ちるのを確認する。
4. 実装する。
5. `mise run verify` を緑にする。
6. [release.md](release.md) の手順でタグとリリースを作る。

2 を飛ばして 4 の後にテストを書くと、実装がどうであれ通るテストができる。それは何も
保証しない。
