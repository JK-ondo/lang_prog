# 内部仕様 — 段階1（字句解析）

前提は [architecture.md](architecture.md)。受入基準は
[../../stages/stage1-lexer.md](../../stages/stage1-lexer.md)、対象言語の字句は
[../language.md](../language.md#字句)。

この段階で作るモジュール: `arena` `diag` `options` `source` `token` `lexer`、および `main` の配線。

---

## source — ファイル全文と位置

### 型

```c
#define SOURCE_EOF (-1)

typedef struct {
  const char *filename;
  const char *text;      /* アリーナ上の全文。NUL 終端されている */
  size_t      len;       /* NUL を含まない長さ */
  size_t      pos;       /* 次に読む位置。0 <= pos <= len */
  int         line;      /* 次に読む文字の行。1 起点 */
  int         col;       /* 次に読む文字の桁。1 起点 */
} source_t;
```

### 関数

```c
bool source_load(source_t *src, const char *filename, arena_t *a, diag_t *d);
int  source_peek(const source_t *src);          /* 進めずに次の 1 文字。終端なら SOURCE_EOF */
int  source_peek2(const source_t *src);         /* 進めずに 2 文字目。最長一致の判定に使う */
int  source_get(source_t *src);                 /* 1 文字進める。行・桁を更新する */
```

### 契約

| 関数 | 事前条件 | 事後条件 |
| ---- | -------- | -------- |
| `source_load` | `src` は未初期化でよい | 成功なら `text` が全文を指し、`pos=0` `line=1` `col=1`。失敗なら `diag_fatal` を呼んで `false` を返す（`src` の中身は不定） |
| `source_peek` | `source_load` 済み | 状態を変えない |
| `source_get` | 同上 | `pos` が 1 文字進み、行・桁が更新される。終端では `pos` を進めず `SOURCE_EOF` を返し続ける |

### 戻しが要らない理由

全文を配列で持ち `pos` で位置を示すので、「1 文字読んで戻す」は `peek` で覗くだけになる。
**`ungetc` も自前の 1 文字バッファも不要。** 先読みが 2 文字必要なところ（`<=` の判定）も
`source_peek2` で足りる。

これは全文をメモリに読む設計を選んだ最大の見返り。逐次読みだと、戻しのバッファと行・桁の
巻き戻しを自分で管理することになり、そこがバグの巣になる。

### 行と桁の更新

`source_get` が読んだ文字に応じて:

| 読んだ文字 | 更新 |
| ---------- | ---- |
| `\n` | `line++`、`col = 1` |
| `\r` で次が `\n` | `\r` と `\n` をまとめて 1 文字分消費し、`\n` を返す。`line++`、`col = 1` |
| `\r` 単独 | 改行として扱い `\n` を返す «要確認» |
| `\t` | `col++`（タブ幅で展開しない。外部仕様 §5 の決定） |
| その他 | `col++` |

境界:

- **空ファイル** — `len == 0`。最初の `peek` が即 `SOURCE_EOF`。
- **末尾に改行がないファイル** — 最後のトークンの後、次の `peek` が `SOURCE_EOF`。特別扱い不要。
- **NUL バイトがファイル中にある** — `len` で長さを持つので読み進めるが、MPPL の文字ではないので
  字句エラーになる «要確認: 課題文が触れているか»。

---

## token — トークンの型と表

### 種類

MPPL のトークンは 49 種類。これに終端を加えて enum は **50 値**になる。

```c
typedef enum {
  TOK_EOF = 0,

  /* 予約語 28（language.md の表と同じ順） */
  TOK_AND, TOK_ARRAY, TOK_BEGIN, TOK_BOOLEAN, TOK_BREAK, TOK_CALL, TOK_CHAR,
  TOK_DIV, TOK_DO, TOK_ELSE, TOK_END, TOK_FALSE, TOK_IF, TOK_INTEGER, TOK_NOT,
  TOK_OF, TOK_OR, TOK_PROCEDURE, TOK_PROGRAM, TOK_READ, TOK_READLN, TOK_RETURN,
  TOK_THEN, TOK_TRUE, TOK_VAR, TOK_WHILE, TOK_WRITE, TOK_WRITELN,

  /* 記号 18 */
  TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_EQUAL, TOK_NOTEQ, TOK_LE, TOK_LEEQ,
  TOK_GR, TOK_GREQ, TOK_LPAREN, TOK_RPAREN, TOK_LBRACKET, TOK_RBRACKET,
  TOK_ASSIGN, TOK_DOT, TOK_COMMA, TOK_COLON, TOK_SEMI,

  /* その他 3 */
  TOK_NAME, TOK_NUMBER, TOK_STRING,

  TOK_KIND_COUNT
} token_kind_t;
```

**`TOK_EOF` は 49 種類に含まれない。** 段階1 の出力で種類ごとの出現回数を数えるとき、`TOK_EOF`
を数に入れるかどうかは課題文の指定に従う «要確認»。

### トークン

```c
typedef struct {
  token_kind_t kind;
  int          line, col;  /* トークンの先頭の位置。1 起点 */
  const char  *text;       /* 綴り。source を指す。NUL 終端されていない */
  size_t       len;        /* 綴りの長さ */
  union {
    unsigned number;                              /* TOK_NUMBER: 0..32767 */
    struct { const char *s; size_t len; } str;     /* TOK_STRING: '' を戻した中身 */
  } value;
} token_t;
```

所有権（[ADR 0002](../../adr/0002-arena-allocation.md)）:

- `text` は**必ず `source` のバッファを指す**。複製しない。NUL 終端されていないので、扱うときは
  常に `len` と対で渡す。`printf` は `%.*s` を使う。
- `value.str` も原則 `source` を指す。**例外は `''` を含む文字列**で、これだけは中身が綴りと違う
  のでアリーナ上に複製を作る。呼ぶ側はどちらか区別しなくてよい（どちらもポインタ + 長さ）。
- `token_t` は値としてコピーしてよい。ポインタ 2 本と整数だけなので安い。

### 関数

```c
const char  *token_kind_name(token_kind_t kind);   /* 種類名。dump と診断が使う */
token_kind_t token_keyword_lookup(const char *s, size_t len);  /* 予約語でなければ TOK_NAME */
```

### enum と名前表がずれる事故を止める

名前表は enum と同じ順で持ち、要素数をコンパイル時に固定する。

```c
static const char *const kind_names[] = { "EOF", "and", "array", /* ... */ };
static_assert(sizeof kind_names / sizeof kind_names[0] == TOK_KIND_COUNT,
              "kind_names が token_kind_t と食い違っている");
```

これを入れないと、enum に 1 つ足したときに名前表がずれ、**テストは通るのに診断だけ嘘をつく**と
いう最も見つけにくい壊れ方をする。

予約語の引き方は線形探索 28 件で十分。トークン数に対して定数回なので速度は問題にならない。

---

## lexer — 字句解析

### 型

```c
typedef struct {
  source_t *src;
  diag_t   *diag;
  arena_t  *arena;
} lexer_t;
```

### 関数

```c
void    lexer_init(lexer_t *lx, source_t *src, diag_t *d, arena_t *a);
token_t lexer_next(lexer_t *lx);
```

### 契約

| | |
| --- | --- |
| 事前条件 | `src` は `source_load` 済み |
| 事後条件 | 返すトークンは必ず位置を持つ。`TOK_EOF` を返した後は、何度呼んでも `TOK_EOF` |
| エラー時 | `diag_error` に記録し、問題の文字を飛ばして**次のトークンを返す**（[ADR 0003](../../adr/0003-continue-after-errors.md)） |

**`TOK_ERROR` という種類は作らない。** エラーを記録して続行すると決めたので、`lexer_next` は
常に正当なトークンか `TOK_EOF` を返す。字句エラーがあったかどうかは `diag_error_count()` で
分かる。こうすると段階2 のパーサがエラートークンを扱わずに済む。

### 1 トークン切り出す手順

```
1. 分離子が無くなるまで読み飛ばす（空白・タブ・改行・注釈の 2 形式）
     └ 注釈が閉じないまま終端に達したらエラー
2. 現在位置を控える ← これがトークンの先頭の位置
3. peek した 1 文字で分岐
     終端            → TOK_EOF
     英字            → 名前を読み、token_keyword_lookup で予約語か判定
     数字            → 整数を読み、32767 を超えたらエラー
     '               → 文字列を読む
     < > :           → peek2 して 2 文字記号か 1 文字記号かを決める
     その他の記号    → 1 文字記号
     どれでもない    → 字句エラー。1 文字飛ばして 1 に戻る
```

手順 1 が**ループ**であることが要点。`{ 注釈 } 空白 /* 注釈 */ 空白` のように分離子が交互に
続くので、「分離子が 1 つも無くなるまで」回す。1 回しか読み飛ばさない実装は、注釈の直後に
空白があると壊れる。

手順 2 の位置取りも間違えやすい。分離子を飛ばした**後**に控える。先に控えると、トークンの
位置が直前の空白を指してしまう。

### 各読み取りの注意点

| 対象 | 注意点 |
| ---- | ------ |
| 名前 | 英字で始まり英数字が続く。最長一致なので `readln` が `read` + `ln` に割れない |
| 整数 | 桁を足しながら 32767 を超えた時点でエラーにする。全部読んでから変換すると `99999999999999` で `unsigned` があふれる。**あふれさせてから検査するのでは遅い** |
| 記号 | `<` は次が `=` なら `TOK_LEEQ`、`>` なら `TOK_NOTEQ`、それ以外なら `TOK_LE`。`>` は次が `=` なら `TOK_GREQ`。`:` は次が `=` なら `TOK_ASSIGN` |
| 文字列 | `'` を読んだら、次の `'` を探す。見つけた `'` の次も `'` なら 1 文字分として飲み込んで続行。改行か終端に達したらエラー。`''` を含むときだけアリーナに複製を作る |
| 注釈 | `{` なら `}` を探す。`/` で次が `*` なら `*/` を探す。`/` で次が `*` でなければ字句エラー（MPPL に `/` 単独の意味はない）。入れ子を数えない |

### 字句エラーの一覧

| 状況 | 位置はどこを指すか |
| ---- | ------------------ |
| MPPL に無い文字 | その文字 |
| 閉じていない文字列 | 開き `'` |
| 文字列の中の改行 | 開き `'` |
| 閉じていない注釈 | 開き `{` または `/*` |
| 整数が 32767 を超える | 数値の先頭 |
| `/` の次が `*` でない | `/` |

「開きの位置を指す」のは、閉じ忘れは開いた場所を見に行くしかないため。終端の位置を指しても
直せない。

---

## options — argv の解析

### 型

```c
typedef struct {
  const char *input;          /* 入力ファイル。NULL なら未指定 */
  const char *output;         /* -o。NULL なら input から導く */
  phase_t     requested;      /* 要求フェーズ */
  bool        dump_tokens, dump_ast, dump_symtab;
  bool        syntax_only, verbose, save_temps, debug_lines, verbose_asm;
  bool        no_warnings, warnings_are_errors;
  unsigned    max_errors;
  /* 個別の警告、-O、-fsanitize など段階ごとに足す */
} options_t;

bool options_parse(options_t *opt, int argc, char **argv, diag_t *d);
```

### 注意点

- **`getopt` は使えない。** 外部仕様 §2 でオプションがファイル名の前後どちらでもよいと決めた。
  GNU の `getopt` は並べ替えをするが、`--dump-tokens` のような長い独自オプションと混ぜると
  扱いが面倒。argv を素直に前から見るループを書く方が短く、振る舞いが読める。
- 入力ファイルは「`-` で始まらない引数」。**2 つ目が来たら記録して読み進め**、全部読み終えて
  から「2 つ以上あった」とエラーにする。最初に見つけた時点でエラーにすると、外部仕様 §3.1 の
  メッセージ（両方のファイル名を挙げる）が作れない。
- `-o` の直後の語が無い（`mpplc -o`）→ エラー。
- `-fmax-errors=` は `=` の後を数値に変換する。空・非数値・負数はエラー。
- 未知オプション → `mpplc: error: unrecognized command-line option '-Xfoo'`。
- 出力名の導出は `.mpl` → `.csl`。拡張子が `.mpl` でないときの導出規則 «要確認: 末尾に足すか、
  最後の `.` 以降を置き換えるか»。
- 要求フェーズは、指定された dump 系のうち**最も深いもの**（[architecture.md](architecture.md#フェーズの配線)）。

### 未確定 «要確認»

- `--version` が出す文字列
- `--` の扱い（外部仕様に記述がない）
- 拡張子警告を抑止するオプション名。`-w` で消えるべきか

---

## main — 配線

流れは [architecture.md](architecture.md#フェーズの配線) の 8 手順。段階1 の時点では手順 6 で
走らせるのは字句解析だけ。

`--dump-tokens` が指定されていたら、`lexer_next` を `TOK_EOF` まで回して 1 つずつ出力する。
出力書式は未確定 «要確認: 課題文の指定»。暫定案:

```
行:桁  種類名  綴り
```

---

## 段階1 で決めないこと

段階2 に残す。ここで作り込むと無駄になる。

- **先読み 1 トークンのバッファ** — パーサが持つ。`lexer` は「次を 1 つ返す」だけでよい。
- **トークン列の保存** — しない。逐次で足りる。
- **`ast.h`** — 段階2 で作る。

## テストとの対応

| テストの置き場 | 何を確かめるか |
| -------------- | -------------- |
| `tests/cases/cli/` | `options` と `main` の配線。`--help` `--version`、入力 0 個 / 2 個以上、不明オプション、未実装エラー |
| `tests/cases/stage1-lexer/` | `--dump-tokens` の出力。49 種類の最小例、境界、字句エラー 6 種 |

`source` と `token` は単体では外から見えないので、`--dump-tokens` の出力を通して確かめる。C に
単体テストの枠組みを入れていないため、これは意図した割り切り。行・桁の正しさは「複数行・タブ・
注釈を挟んだ後のトークンの位置」で見る。
