# 開発の回し方

## 前提

ツールチェーンは mise で固定されている。この 2 つが必要:

| 用途           | ツール                                    | 状態                     |
| -------------- | ----------------------------------------- | ------------------------ |
| コンパイル     | Apple clang (`/usr/bin/clang`)            | OS 同梱                  |
| ビルド生成     | cmake (mise) + ninja                      | 導入済み                 |
| 整形           | `clang-format`                            | **未導入**（下記参照）   |
| 静的解析       | `clang --analyze`                         | Apple clang 同梱         |

`clang-format` はこのマシンに入っていない。そのため **`verify:fmt` は検証一式から外してある**
（`mise.toml` の `[tasks.verify]` の `depends` を見ること）。タスク自体は残っているので、
`mise run verify:fmt` で単独に走らせられる。

導入するときは、割り当てを確認してから**自分で**実行する（このリポジトリではツールを勝手に
入れない方針）。

```sh
mise registry clang-format     # どのバックエンドに割り当てられているか
mise ls-remote clang-format    # 版を引けるか（入れずに確認できる）
mise use -g clang-format@latest
```

入ったら `[tasks.verify]` の `depends` を `["verify:*"]` に戻す。それまで整形検査は効かない。

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

| leaf           | 中身                                                   | `verify` に含まれるか |
| -------------- | ------------------------------------------------------ | --------------------- |
| `verify:lint`  | `clang --analyze`（未初期化・NULL 参照・リークを見る） | はい                  |
| `verify:build` | `-Werror` 付きの Release ビルド                        | はい                  |
| `verify:test`  | `ctest`（ASan/UBSan 有効のバイナリで）                 | はい                  |
| `verify:fmt`   | `clang-format --dry-run --Werror`                      | **いいえ**（未導入）  |

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
