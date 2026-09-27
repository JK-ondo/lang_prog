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

## コーディング規約

### `const` は付けられる限り付ける

MISRA C:2012 Rule 8.13（「ポインタは可能なかぎり const 修飾された型を指すべき」）に倣う。
`const` が付いていないポインタは「この関数はこれを書き換える」という意思表示になり、逆に
付いていれば読むだけだと一目で分かる。**付け忘れは情報の欠落**として扱う。

| 場面 | 書き方 |
| ---- | ------ |
| 読むだけのポインタ引数 | `void f(const source_t *src)` |
| 読むだけの文字列引数 | `void f(const char *name, size_t len)` |
| 返す読み取り専用の文字列 | `const char *token_kind_name(token_kind_t k)` |
| ファイル内の表 | `static const char *const kind_names[] = { ... }` |
| 書き換えない局所変数 | `const size_t len = tok.len;` |

表は**2 段とも** `const` にする。`static const char *names[]` は「要素を差し替えられる表」であり、
意図と違う。

書き換える引数には付けない。`diag_error(diag_t *d, ...)` の `d` は件数を増やすので `const` に
できない。**`const` の有無がそのまま「この関数は状態を変えるか」の一覧になる**のが狙い。

### MISRA C を尊重する。ただし逸脱を記録する

MISRA C:2012 の規則のうち、このプロジェクトで守れるものは守る。**守れないものは
[ADR 0004](../../adr/0004-misra-c-with-deviations.md) に逸脱として記録する。** MISRA 自身が
「文書化された逸脱を伴う適合」という形を認めているので、これが正しい付き合い方になる。

黙って破らないこと。破る理由を書けないなら、まだ設計が決まっていない。

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

### 型

`diag_t` の中身はヘッダに書く。`main` が `diag_t d;` とスタックに置けるので、割当てが 1 件も
発生せず、「明示的に解放する資源は `FILE*` だけ」という規約に例外を作らない。中身を他の
モジュールが触らないのは規約で守る（触る必要がない）。

```c
/* 診断メッセージの最大長。超えた分は切り捨てる。診断が切れてもコンパイル結果は変わらない。 */
#define DIAG_MSG_MAX 512

/* 診断に出すプログラム名。位置を持たない診断の行頭に付く。 */
#define DIAG_PROG_NAME "mpplc"

typedef struct {
  const char *filename;            /* 診断に出すファイル名。所有しない */
  bool        no_warnings;         /* -w */
  bool        warnings_are_errors; /* -Werror */
  unsigned    max_errors;          /* -fmax-errors=n。0 は無制限 */
} diag_config_t;

typedef struct {
  diag_config_t cfg;            /* diag_init で複製する */
  FILE         *out;            /* 出力先。所有しない */
  unsigned      errors;         /* エラー件数 */
  bool          limit_reported; /* 打ち切りの行を出したか */
} diag_t;
```

警告の件数は持たない。`-Werror` は警告を `errors` に数えるだけで足り、警告件数を読む相手が
いないため（MISRA 2.x の「使わないものを残さない」に従う）。

### 関数

```c
void diag_init(diag_t *d, const diag_config_t *cfg, FILE *out);

void diag_error(diag_t *d, int line, int col, const char *msg);
void diag_warning(diag_t *d, int line, int col, const char *opt, const char *msg);
void diag_fatal(diag_t *d, const char *msg);

unsigned diag_error_count(const diag_t *d);
bool     diag_should_stop(const diag_t *d);
```

**可変長引数を使わない**（MISRA 17.1）。メッセージは組み立て済みの文字列として受け取る。

### 呼び側がメッセージを組む

呼ぶ側が `snprintf` で局所バッファに組んでから渡す。

```c
char msg[DIAG_MSG_MAX];
(void)snprintf(msg, sizeof msg, "unexpected character '%c'", c);
diag_error(d, line, col, msg);
```

この形を選んだ見返りが 2 つある。

- **書式の型検査が無料で付く。** `snprintf` は libc の既知の関数なので、clang が
  `__attribute__((format))` の指定なしに書式と引数の対応を検査する。自分で可変長引数の関数を
  作ると、この検査を自分で要求しなければならない。
- `diag` 側に書式の解釈が一切要らない。

代償は呼び出しが 2 行になることと、各呼び出し点にバッファが立つこと。切り捨ては診断にしか
影響しないので許容する（`snprintf` は必ず NUL 終端する）。

トークンの綴りは NUL 終端されていないので、`%.*s` と `(int)len` の対で渡す。

```c
(void)snprintf(msg, sizeof msg, "number out of range: %.*s", (int)tok.len, tok.text);
```

### 契約

| 関数 | 事前条件 | 事後条件 |
| ---- | -------- | -------- |
| `diag_init` | `out` は NULL でない。`cfg->filename` は `d` より長生き | `cfg` を複製し、件数を 0 にする |
| `diag_error` | `diag_init` 済み | 件数を必ず 1 増やす。`-w` の影響を受けない（警告は消せてもエラーは消せない） |
| `diag_warning` | 同上、`opt` は NULL でない | `-w` なら何も出さず件数も増やさない。`-Werror` ならエラーとして数える |
| `diag_fatal` | 同上 | 位置を持たない診断を出し、件数を 1 増やす |
| `diag_should_stop` | 同上 | `max_errors != 0 && errors >= max_errors` |

`diag_warning` の `opt` は**省略できない**。すべての警告に `[-Wxxx]` が付き、使う人が「この
警告を消すには何を指定すればよいか」を常に知れる。新しい警告を足すときにオプション名を考える
ことが強制されるのも狙い。

`-Werror` で昇格した警告は `[-Werror=xxx]` と出す（gcc に倣う）。指定の由来が分かる。

### `-fmax-errors` に達したとき

`diag` は**プロセスを終わらせない**（MISRA 21.8。`exit` を呼ぶのはアリーナの確保失敗だけ）。

1. 上限に達した時点で打ち切りの行を 1 度だけ出す «要確認: 文言。gcc は
   `compilation terminated due to -fmax-errors=N.`»
2. 以降の `diag_error` は**何も出さない**（件数は数える）
3. フェーズ側は `diag_should_stop` を見て早めに抜ける

3 を実装しなくても出力は正しい。抜けるのは無駄な仕事を省くためだけ。進退を決めるのは `main`
であって `diag` ではない（[段階間の約束](#段階間の約束) の 1）。

### 唯一の出力経路

**stderr へ書くのは `diag` だけ。** 他のモジュールは `fprintf(stderr, ...)` を呼ばない。診断の
書式が 1 箇所で決まるのはこの規約のおかげ。書式そのものは外部仕様
[§5](../compiler-spec.md#5-診断メッセージの形式) が正。

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
| `-S` | `PHASE_CODEGEN` |
| **指定なし（既定）** | **`MPPLC_LAST_IMPLEMENTED_PHASE`** |

複数指定されたら**最も深いもの**が要求フェーズになる。`--dump-tokens --dump-ast` なら
`PHASE_PARSE` まで走り、両方を出力する。

### 既定が「実装済みの最終フェーズ」である理由

各段階の課題は「そのプログラムを引数 1 つで走らせたときの標準出力」を提出物として指定している。
既定を `PHASE_CODEGEN` に固定すると、段階1〜3 の間は `./mpplc foo.mpl` が「未実装」で落ちて、
課題の実行方法がそのまま通らない。

既定を定数に結びつけると、段階が進むにつれて既定の成果物が自動的に移る。

| 段階 | 定数 | `./mpplc foo.mpl` が出すもの |
| ---- | ---- | ---------------------------- |
| 1 | `PHASE_LEX` | 字句の出現数の表 |
| 2 | `PHASE_PARSE` | 整形出力（予定） |
| 3 | `PHASE_SEMANTIC` | 交差参照表（予定） |
| 4 | `PHASE_CODEGEN` | CASL II（`-o` かファイルへ） |

**未実装エラーは「明示的に深いフェーズを要求したとき」だけ出る。** 段階1 の時点で `--dump-ast`
を渡すと `mpplc: error: parsing is not implemented yet` になる。既定では出ない。

段階が進むたびの変更は、やはり「定数を 1 つ進める」「フェーズ関数を 1 本足す」の 2 つだけ。

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
