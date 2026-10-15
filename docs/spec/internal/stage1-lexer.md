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
| `source_load` | `src` は未初期化でよい | 成功なら `text` が全文を指し、`pos=0` `line=1` `col=1`。失敗なら `diag_error_noloc` を呼んで `false` を返す（`src` の中身は不定） |
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

改行は **4 通り**あり、それぞれを 1 つの改行として扱う（`\r` `\n` `\r\n` `\n\r`）。

| 読んだ文字 | 更新 |
| ---------- | ---- |
| `\r` で次が `\n` | 2 バイトまとめて消費し `\n` を返す。`line++`、`col = 1` |
| `\n` で次が `\r` | 2 バイトまとめて消費し `\n` を返す。`line++`、`col = 1` |
| `\r` 単独 | `\n` を返す。`line++`、`col = 1` |
| `\n` 単独 | `\n` を返す。`line++`、`col = 1` |
| `\t` | `col++`（タブ幅で展開しない。外部仕様 §5 の決定） |
| その他 | `col++` |

`source_get` が改行を必ず `\n` に正規化して返すので、`lexer` は 4 通りを意識しなくてよい。
これが `source` を独立させた見返りの一つ。

境界:

- **空ファイル** — `len == 0`。最初の `peek` が即 `SOURCE_EOF`。
- **末尾に改行がないファイル** — 最後のトークンの後、次の `peek` が `SOURCE_EOF`。特別扱い不要。
- **表示文字でない制御コード** — `len` で長さを持つので読み進める。`lexer` が字句エラーにする
  （[../language.md](../language.md#表示文字) の決定）。`0x00` も同様に扱えるので、NUL バイトを
  特別扱いしない。
- **非常に長い行** — 全文をメモリに持つので行長の上限がない。課題文の「長い行があっても実行時
  エラーを起こしてはいけない」を構造的に満たす。

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
  TOK_GR, TOK_GREQ, TOK_LPAREN, TOK_RPAREN, TOK_LSQPAREN, TOK_RSQPAREN,
  TOK_ASSIGN, TOK_DOT, TOK_COMMA, TOK_COLON, TOK_SEMI,

  /* その他 3 */
  TOK_NAME, TOK_NUMBER, TOK_STRING,

  TOK_KIND_COUNT
} token_kind_t;
```

**`TOK_EOF` は 49 種類に含まれない。** 出現数の表にも出さない（課題文は分離子も EOF も字句では
ないとしている）。`TOK_EOF` を enum の先頭に置いたのは、`0` が「まだ読んでいない」と重なって
都合がよいため。

**enum の並びが出力の並び順になる。** 出現数の表の並びは課題文に指定がないので、この enum を
先頭から舐めて 0 でないものを出す、と決めた。キーワードをアルファベット順に並べてあるのは
そのため（課題文の例が `and`, `array`, … の順であることと矛盾しない）。

### トークン

```c
typedef struct {
  token_kind_t kind;
  int          line, col;  /* トークンの先頭の位置。1 起点 */
  const char  *text;       /* 綴り。source を指す。NUL 終端されていない */
  size_t       len;        /* 綴りの長さ */
  union {
    unsigned number;                              /* TOK_NUMBER: 0..32768 */
    struct { const char *s; size_t len; } str;     /* TOK_STRING: '' を戻した中身 */
  } value;
} token_t;
```

所有権（[ADR 0002](../../adr/0002-arena-allocation.md)）:

- `text` は**必ず `source` のバッファを指す**。複製しない。NUL 終端されていないので、扱うときは
  常に `len` と対で渡す。`printf` は `%.*s` を使う。
- `value.str` も原則 `source` を指す。**例外は `''` を含む文字列**で、これだけは中身が綴りと違う
  のでアリーナ上に複製を作る。呼ぶ側はどちらか区別しなくてよい（どちらもポインタ + 長さ）。
- **`value.str.len` が課題文のいう「文字列の長さ」になる。** `''` を 1 文字に戻した後の長さなので
  定義と一致する（`''''` なら 1）。この値は課題3 の制約規則で使うので、段階1 で正しく作っておく。
- `token_t` は値としてコピーしてよい。ポインタ 2 本と整数だけなので安い。

### 関数

```c
/* 出現数の表に出す綴り。キーワードは "and"、記号は "+"、他は "NAME" "NUMBER" "STRING" */
const char *token_kind_spelling(token_kind_t kind);

/* --dump-tokens に出す記号名。"AND" "PLUS" "NAME" "ASSIGN" … */
const char *token_kind_symbol(token_kind_t kind);

/* 予約語でなければ TOK_NAME を返す */
token_kind_t token_keyword_lookup(const char *s, size_t len);
```

**種類名が 2 通りある**のは、2 つの出力が別の綴りを要求するため。

| 用途 | `TOK_ASSIGN` の場合 | `TOK_NAME` の場合 |
| ---- | ------------------- | ----------------- |
| 出現数の表（課題1 の提出物） | `:=` | `NAME` |
| `--dump-tokens`（デバッグ用） | `ASSIGN` | `NAME` |

`token_keyword_lookup` は `token_kind_spelling` の表をそのまま引ける（キーワードの綴りが表に
あるため）。専用の表を作らない。

### enum と 2 つの表がずれる事故を止める

50 個の enum 値に対して名前の配列が 2 本ある。**手で並べると必ずずれる。** ずれると、
テストは通るのに診断や dump だけが嘘をつくという、最も見つけにくい壊れ方をする。

対策は 2 つあり、どちらを採るかは実装時に決める。

**案A: 1 つのリストから enum と 2 つの表を生成する（X リスト）**

```c
/* token.h */
#define TOKEN_KINDS(X)                  \
  X(TOK_EOF,     "EOF",     "EOF")      \
  X(TOK_AND,     "AND",     "and")      \
  X(TOK_ARRAY,   "ARRAY",   "array")    \
  /* … 50 行 … */                       \
  X(TOK_ASSIGN,  "ASSIGN",  ":=")       \
  X(TOK_NAME,    "NAME",    "NAME")

#define X(kind, symbol, spelling) kind,
typedef enum { TOKEN_KINDS(X) TOK_KIND_COUNT } token_kind_t;
#undef X
```

`token.c` で同じ `TOKEN_KINDS(X)` を別の `X` の定義で 2 回展開すれば 2 つの表ができる。
**ずれることが原理的に起きない**のが利点。`##` 演算子を使わないので MISRA 20.10 には触れない
（enum 名を全部書き下すため）。欠点はマクロを読む負担。

**案B: 手で並べて `static_assert` で長さを固定する**

```c
static const char *const kind_symbols[] = { "EOF", "AND", "ARRAY", /* … */ };
static_assert(sizeof kind_symbols / sizeof kind_symbols[0] == TOK_KIND_COUNT,
              "kind_symbols が token_kind_t と食い違っている");
```

読みやすいが、**長さは守れても順序は守れない**。`TOK_AND` と `TOK_ARRAY` を入れ替えても
`static_assert` は通る。長さが合っているという安心が、かえって危ない。

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
     └ 注釈が閉じないまま終端に達したら、そこまでを注釈として扱う（エラーにしない）
2. 現在位置を控える ← これがトークンの先頭の位置
3. peek した 1 文字で分岐
     終端            → TOK_EOF
     英字            → 名前を読み、token_keyword_lookup で予約語か判定
     数字            → 整数を読み、32768 を超えたらエラー
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
| 整数 | 桁を足しながら 32768 を超えた時点でエラーにする。全部読んでから変換すると `99999999999999` で `unsigned` があふれる。**あふれさせてから検査するのでは遅い** |
| 記号 | `<` は次が `=` なら `TOK_LEEQ`、`>` なら `TOK_NOTEQ`、それ以外なら `TOK_LE`。`>` は次が `=` なら `TOK_GREQ`。`:` は次が `=` なら `TOK_ASSIGN` |
| 文字列 | `'` を読んだら、次の `'` を探す。見つけた `'` の次も `'` なら 1 文字分として飲み込んで続行（`''''` が長さ 1 の文字列 1 個になるのはこの動き）。改行か終端に達したらエラー。`''` を含むときだけアリーナに複製を作る。**長さは `''` を 1 と数えた後の文字数**で、これが課題3 で使われる |
| 注釈 | `{` なら `}` を探す。`/` で次が `*` なら `*/` を探す。`/` で次が `*` でなければ字句エラー（MPPL に `/` 単独の意味はない）。入れ子を数えない。**終端に達しても、そこまでを注釈として扱い、エラーにしない** |

### 字句エラーの一覧

| 状況 | 位置はどこを指すか |
| ---- | ------------------ |
| MPPL に無い表示文字 | その文字 |
| 表示文字でない制御コード | その文字 |
| 閉じていない文字列 | 開き `'` |
| 文字列の中の改行 | 開き `'` |
| 符号なし整数が 32768 を超える | 数値の先頭 |
| `/` の次が `*` でない | `/` |

「開きの位置を指す」のは、閉じ忘れは開いた場所を見に行くしかないため。終端の位置を指しても
直せない。

**閉じていない注釈はこの表に無い。** 課題文が「EOF までを注釈として処理する」と明示している
ので、エラーではない。ここは注釈と文字列で扱いが分かれる点なので間違えやすい。

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

typedef enum {
  OPTIONS_OK,       /* 続行する */
  OPTIONS_FINISHED, /* --help / --version を出した。終了コードは 0 */
  OPTIONS_ERROR     /* 診断は済んでいる。終了コードは 1 */
} options_status_t;

options_status_t options_parse(options_t *opt, int argc, char **argv, diag_t *d);
```

**この構造体は段階4 までの分を並べているが、各段階では使うフィールドだけを書く。** 段階1 で
書くのは `input` / `requested` / `dump_tokens` / `no_warnings` / `warnings_are_errors` /
`max_errors` の 6 つ。使わないフィールドを先に置くと、埋める責任のない場所が増える（MISRA 2.x
「使わないものを残さない」）。`-o` を受け付ける段階4 で `output` を足す。

**戻り値が `bool` ではなく 3 値なのは、結末が 3 つあるため。** `--help` と `--version` は
「成功して終了コード 0 で終わる」ので、「続行」とも「エラー」とも違う。`bool` + `options_t` の
フラグでも書けるが、フラグを読み忘れると `--help` が入力ファイルの検証に進んでしまう。
**結末を型に出して、取りこぼしを `switch` の網羅性検査に任せる。**

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
  最後の `.` 以降を置き換えるか»。段階4 まで使わないので後回しにできる。
- 要求フェーズは、指定された dump 系のうち**最も深いもの**（[architecture.md](architecture.md#フェーズの配線)）。
  どれも指定されていなければ `MPPLC_LAST_IMPLEMENTED_PHASE`。
- `--` は不明なオプションとして扱う（外部仕様 §2.4）。特別扱いしない。
- 拡張子が `.mpl` でないときの警告は `-Wbad-file-extension` の名前で出す。既定で有効。

### 段階1 で受け付けるオプション

`--help` に載せるものと一致させる（外部仕様 §2.3）。これ以外を渡したら不明オプションのエラー。

| オプション | `options_t` のどこへ |
| ---------- | -------------------- |
| `--dump-tokens` | `dump_tokens` |
| `-w` | `no_warnings` |
| `-Werror` | `warnings_are_errors` |
| `-fmax-errors=<n>` | `max_errors` |
| `-h` / `--help` | 即座に使い方を出して終了コード 0 |
| `--version` | 即座に `mpplc 0.1.0` を出して終了コード 0 |

`--help` と `--version` は**入力ファイルの検証より先に処理する**。`mpplc --help` は入力ファイル
なしでも成功しなければならない。

### 終了コードの注意

外部仕様 §6 では**コマンドラインの誤りも 1**。現在の `src/main.c` のスタブは引数なしで 2 を
返しているので、ここで 1 に直す。

---

## main — 配線

流れは [architecture.md](architecture.md#フェーズの配線) の 8 手順。段階1 の時点では手順 6 で
走らせるのは字句解析だけ。

段階1 の成果物は**出現数の表**（[../language.md](../language.md#段階1-の出力--字句とその出現数の表)）。
`MPPLC_LAST_IMPLEMENTED_PHASE == PHASE_LEX` なので、オプションなしの `./mpplc foo.mpl` がこれを
標準出力へ出す。

### 出現数の数え方

```c
static unsigned counts[TOK_KIND_COUNT];   /* 種類ごとの出現数 */
```

`lexer_next` を `TOK_EOF` まで回して `counts[tok.kind]++` するだけ。`TOK_EOF` は数えない。
出力は enum を先頭から舐めて `counts[k] != 0` の行だけ出す。

名前の実体ごとの計数は、名前の文字列をキーにした表が別に必要になる。**アリーナ上に持ち、
`source` を指すポインタ + 長さで比較する**（`token_t.text` の規約どおり）。件数が少ないので
線形探索で足りるが、大きな入力で遅いと感じたらハッシュに変える。

### 2 つの出力を混同しないこと

| オプション | 出すもの | 誰のため |
| ---------- | -------- | -------- |
| なし（段階1 の既定） | **出現数の表** | 課題1 の提出物 |
| `--dump-tokens` | 位置つきのトークン列（1 行 1 トークン） | 自分のデバッグ用 |

`--dump-tokens` の書式は[外部仕様 §4.2](../compiler-spec.md#--dump-tokens-の出力書式)で確定した。
タブ区切り、`STRING` だけ長さと引用符つきの中身を出す。

実装で注意する点が 2 つある。

- **タブ区切り**なので `printf` の書式に空白の桁揃えを入れない。`"%d:%d\t%s\t..."`。
- `STRING` の中身は `\'` `\t` `\\` をエスケープして出す。この 3 つで尽きる（改行は文字列に
  入れられず、タブ以外の制御コードは字句エラーになるため）。

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
