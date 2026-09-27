# 内部仕様 — 全体構成

4 段階を通して変わらないものだけを書く。段階ごとの内部構造は `stage1-lexer.md` 以降にある。

## この文書の位置づけ

| 文書 | 答える問い |
| ---- | ---------- |
| [../language.md](../language.md) | 入力となる MPPL とは何か |
| [../compiler-spec.md](../compiler-spec.md) | 使う人から見てどう振る舞うか（CLI・診断・終了コード） |
| [../../stages/](../../stages/) | 各段階で何をどこまで作るか、受入基準は何か |
| **この文書** | **どう作るか。モジュールの境界・依存方向・データ構造・関数の契約** |

外から見える振る舞いを変えるときは外部仕様を、内部の作りを変えるときはこちらを直す。両方に
同じことを書かない。

## データの流れ

```
foo.mpl
  │  source        ファイル全文をメモリへ。行・桁を管理
  ▼
lexer  ──►  token_t を 1 つずつ                      段階1
  │
  ▼  parser
ast_t                                                 段階2
  │
  ▼  semantic
ast_t（型注釈済み） + symtab_t                        段階3
  │
  ▼  codegen
foo.csl                                               段階4
```

全フェーズが横断して使うもの:

| | 役割 | 向き |
| --- | ---- | ---- |
| `options_t` | 起動時に決まった設定 | 読むだけ |
| `diag_t` | 診断の出力と集計 | 書き込む |
| `arena_t` | 記憶域の確保 | 取るだけ（返さない） |

## モジュールと依存方向

**依存は一方向。** 上の層は下の層を include してよく、逆は禁止。循環参照を作りたくなったら、
境界の引き方が間違っている。

| 層 | モジュール | include してよい先 | 段階 |
| -- | ---------- | ------------------ | ---- |
| 4 | `main` | すべて | 1 |
| 3 | `codegen` | `ast` `symtab` `diag` `arena` `options` | 4 |
| 3 | `semantic` | `ast` `symtab` `diag` `arena` | 3 |
| 3 | `parser` | `lexer` `token` `ast` `diag` `arena` | 2 |
| 2 | `lexer` | `source` `token` `diag` `arena` | 1 |
| 1 | `source` | `diag` `arena` | 1 |
| 1 | `token` | **何も** | 1 |
| 0 | `diag` | — | 1 |
| 0 | `arena` | — | 1 |
| 0 | `options` | — | 1 |

守るべき点が 3 つある。

- **`token.h` は何も include しない。** enum と struct だけ。ここが崩れると、段階2 のパーサが
  `lexer` を引きずり込む。
- **`diag` は `options` を include しない。** 必要な設定（`-w` `-Werror` `-fmax-errors`）は
  初期化時に値として受け取る。こうすると 0 層に循環が生まれない。
- 各 `.c` は自分の `.h` を**最初に** include する。ヘッダが自己完結していないと、そこで落ちる。

## 命名規約

| 対象 | 規約 | 例 |
| ---- | ---- | -- |
| 型 | `snake_case_t` | `token_t` `arena_t` |
| 公開関数 | `モジュール名_動詞` | `lexer_next` `diag_error` |
| モジュール内部だけの関数 | `static`、接頭辞なし | `static int read_name(...)` |
| enum の値 | 大文字 + 接頭辞 | `TOK_NAME` `PHASE_LEX` |
| ヘッダガード | `MPPLC_<ファイル名>_H` | `MPPLC_TOKEN_H` |
| 真偽値 | `<stdbool.h>` の `bool` | — |

## 記憶域 — アリーナ

**決定: アリーナで一括確保・一括解放する**（[ADR 0002](../../adr/0002-arena-allocation.md)）。

```c
typedef struct arena arena_t;

arena_t *arena_create(void);
void    *arena_alloc(arena_t *a, size_t size);            /* 整列済み。失敗しない */
char    *arena_strndup(arena_t *a, const char *s, size_t n);
void     arena_destroy(arena_t *a);
```

規約:

- **アリーナから取った領域を個別に free しない。** `arena_destroy` が全部返す。
- `arena_alloc` は `max_align_t` 境界に整列した領域を返す。
- **確保に失敗したら `mpplc: error: out of memory` を出して終了コード 1 で死ぬ。** 呼び出し側は
  NULL 検査をしない。これは失敗を無視しているのではなく、「回復できない失敗は 1 箇所で扱う」と
  決めたということ。コンパイラは 1 回走って終わるので、部分的に続行する意味がない。
- アリーナは `main` が 1 つ作り、全フェーズに渡す。フェーズごとに分けない。
- ソースの全文バッファも AST も記号表も文字列も、すべてアリーナから取る。**明示的に解放する
  資源は `FILE*` だけ**。

`FILE*` の扱い:

| 用途 | 規約 |
| ---- | ---- |
| 入力 | 全文を読んだら**すぐ閉じる**。読み取り失敗の経路でも閉じる |
| 出力 | 一時ファイルに書き、成功時に最終名へ `rename` する。失敗時は一時ファイルを `remove`。
これで外部仕様 §6 の「エラーなら出力ファイルを作らない」が自然に満たされ、壊れた `.csl` が
残らない |

## 診断 — diag

```c
typedef struct diag diag_t;

typedef struct {
  const char *filename;           /* 診断に出すファイル名 */
  bool        no_warnings;         /* -w */
  bool        warnings_are_errors; /* -Werror */
  unsigned    max_errors;          /* -fmax-errors=n。0 は無制限 */
} diag_config_t;

void     diag_init(diag_t *d, const diag_config_t *cfg, FILE *out);
void     diag_error(diag_t *d, int line, int col, const char *fmt, ...);
void     diag_warning(diag_t *d, int line, int col, const char *opt, const char *fmt, ...);
void     diag_fatal(diag_t *d, const char *fmt, ...);   /* 位置を持たない。mpplc: error: */
unsigned diag_error_count(const diag_t *d);
bool     diag_should_stop(const diag_t *d);             /* -fmax-errors に達した */
```

契約:

- `diag_error` は必ずエラー件数を増やす。`-w` の影響を受けない（警告は消せるがエラーは消せない）。
- `diag_warning` は `-w` なら何も出さず件数も増やさない。`-Werror` ならエラーとして数える。
  `opt` は `[-Wunused-variable]` として出す名前。省略できない。
- `diag_fatal` はコマンドラインやファイル入出力の失敗に使う。行・桁を持たない。
- **stderr へ書くのは `diag` だけ。** 他のモジュールは `fprintf(stderr, ...)` を呼ばない。
  診断の書式が 1 箇所で決まるのはこの規約のおかげ。
- `-fmax-errors` に達したら、打ち切った旨を 1 度だけ出して以降の `diag_error` は黙る。フェーズ
  側は `diag_should_stop` を見て早めに抜ける «要確認: 文言。gcc は
  `compilation terminated due to -fmax-errors=N.`»。

書式そのものは外部仕様 [§5](../compiler-spec.md#5-診断メッセージの形式) が正。

## フェーズの配線

```c
typedef enum { PHASE_LEX, PHASE_PARSE, PHASE_SEMANTIC, PHASE_CODEGEN } phase_t;

/* 実装が進むたびに 1 つ進める。未実装の判定がここ 1 箇所に集まる。 */
#define MPPLC_LAST_IMPLEMENTED_PHASE PHASE_LEX
```

要求フェーズは `options_t` から決める。

| 指定 | 要求フェーズ |
| ---- | ------------ |
| `--dump-tokens` | `PHASE_LEX` |
| `--dump-ast` | `PHASE_PARSE` |
| `--dump-symtab` / `-fsyntax-only` | `PHASE_SEMANTIC` |
| 既定 / `-S` | `PHASE_CODEGEN` |

複数指定されたら**最も深いもの**が要求フェーズになる。`--dump-tokens --dump-ast` なら
`PHASE_PARSE` まで走り、両方を出力する。

`main` の流れ:

1. `options` を解析。失敗 → `diag_fatal` → 1
2. 入力ファイル数と拡張子を検証（0 個 / 2 個以上 → 1）
3. `arena` を作り、`diag` を初期化する
4. `source` を読み込む。失敗 → 1
5. 要求フェーズを決める
6. 実装済みフェーズまで順に走らせる。各フェーズの後で `diag_error_count() > 0` なら中断
7. 要求フェーズ > `MPPLC_LAST_IMPLEMENTED_PHASE` なら「未実装」を `diag_fatal` → 1
8. 終了コードは、エラー件数 > 0 なら 1、なければ 0

段階が進むたびの変更は**「定数を 1 つ進める」「フェーズ関数を 1 本足す」の 2 つだけ**。スタブが
後で消すゴミにならないのが、この形を選んだ理由。

## 段階間の約束

破ると後続の段階が壊れる。

| # | 約束 | これを守ると |
| - | ---- | ------------ |
| 1 | エラーが 1 件でもあれば次のフェーズに進まない | 各フェーズは「入力は前のフェーズの契約を満たす」と仮定できる |
| 2 | 段階3 を通った AST は、すべての名前が解決済みで、すべての式に型が付く | 段階4 は記号表を探し直さない |
| 3 | すべてのトークンと AST 節点が位置（行, 桁）を持つ | どのフェーズでも診断を出せる |
| 4 | stderr へ書くのは `diag` だけ | 診断の書式が 1 箇所で決まる |
| 5 | 出力ファイルは成功時にのみ最終名で存在する | 壊れた `.csl` が残らない |
| 6 | 字句・構文・意味のエラーは記録して**続行**する | `-fmax-errors` が意味を持ち、1 回の実行で複数の誤りが分かる（[ADR 0003](../../adr/0003-continue-after-errors.md)） |

## ファイル構成

```
src/
  main.c                 起動、フェーズの配線、終了コード
  arena.h    arena.c     記憶域                          段階1
  diag.h     diag.c      診断                            段階1
  options.h  options.c   argv の解析                     段階1
  source.h   source.c    ファイル全文と位置              段階1
  token.h    token.c     トークンの型と表                段階1
  lexer.h    lexer.c     字句解析                        段階1
  ast.h      ast.c       構文木                          段階2
  parser.h   parser.c    構文解析                        段階2
  symtab.h   symtab.c    記号表                          段階3
  semantic.h semantic.c  意味解析                        段階3
  codegen.h  codegen.c   CASL2 生成                      段階4
```

ヘッダはソースと同じ場所に置く。`src/*.c` は CMake が `CONFIGURE_DEPENDS` の glob で拾うので、
ファイルを足しても `CMakeLists.txt` は触らない。

## ここを変えるとき

次の 3 つを覆すときは ADR を足す。他のすべてがこれらに乗っているため。

1. 依存方向（層の表）
2. 記憶域はアリーナ
3. stderr へ書くのは `diag` だけ
