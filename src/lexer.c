#include "lexer.h"

static bool is_letter(int c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool is_digit(int c) {
  return c >= '0' && c <= '9';
}

static bool is_name_char(int c) {
  return is_letter(c) || is_digit(c);
}

/*
 * 表示文字（language.md の定義）= タブ、改行、および 0x20〜0x7e
 */
static bool is_graphic(int c) {
  return c == '\t' || c == '\n' || (c >= 0x20 && c <= 0x7e);
}

/*
 * MPPLにない文字を報告する（外部仕様§5.2）
 * 事前条件：問題の文字は呼ぶ側が消化済み。tokは位置を持っている。
 */
static void report_bad_char(lexer_t *lx, const token_t *tok, int c) {
  char msg[DIAG_MSG_MAX];
  if (c == '/') {
    (void)snprintf(msg, sizeof msg, "'/' must be followed by '*' to start a comment");
  } else if (c >= 0x80) {
    (void)snprintf(msg, sizeof msg, "byte 0x%02x cannot be used in MPPL", (unsigned)c);
  } else if (!is_graphic(c)) {
    (void)snprintf(msg, sizeof msg, "control character 0x%02x cannot be used in MPPL", (unsigned)c);
  } else {
    (void)snprintf(msg, sizeof msg, "'%c' cannot be used in MPPL", (char)c);
  }
  diag_error(lx->diag, tok->line, tok->col, msg);
}

/*
 * 初期化
 */
void lexer_init(lexer_t *lx, source_t *src, arena_t *arena, diag_t *diag) {
  lx->src = src;
  lx->arena = arena;
  lx->diag = diag;
}

/*
 * 識別子を読み取る
 * 事前条件：トークンの先頭が英字である
 */
static token_t lex_name(lexer_t *lx, token_t tok) {
  const char *start = lx->src->text + lx->src->pos;
  while (is_name_char(source_peek(lx->src))) {
    (void)source_get(lx->src);
  }
  tok.text = start;
  tok.len = (size_t)(lx->src->text + lx->src->pos - start);
  tok.kind = token_keyword_lookup(start, tok.len);
  return tok;
}

/*
 * 数値を読み取る
 * 事前条件：トークンの先頭が数字である
 */
static token_t lex_number(lexer_t *lx, token_t tok) {
  const char *start = lx->src->text + lx->src->pos;
  unsigned value = 0;
  bool over = false;
  while (is_digit(source_peek(lx->src))) {
    int c = source_get(lx->src);
    if (!over) {
      value = value * 10u + (unsigned)(c - '0');
      if (value > MPPL_NUMBER_MAX) {
        over = true;
      }
    }
  }
  tok.kind = TOK_NUMBER;
  tok.text = start;
  tok.len = (size_t)(lx->src->text + lx->src->pos - start);
  tok.value.number = over ? 0u : value;
  if (over) {
    char msg[DIAG_MSG_MAX];
    (void)snprintf(msg, sizeof msg, "number out of range: %.*s", (int)tok.len, tok.text);
    diag_error(lx->diag, tok.line, tok.col, msg);
  }
  return tok;
}

/*
 * 記号を読み取る
 * 事前条件：終端でない、英数字でない
 */
static token_t lex_symbol(lexer_t *lx, token_t tok) {
  const char *start = lx->src->text + lx->src->pos;
  int c = source_get(lx->src);

  switch (c) {
  case '+':
    tok.kind = TOK_PLUS;
    break;
  case '-':
    tok.kind = TOK_MINUS;
    break;
  case '*':
    tok.kind = TOK_STAR;
    break;
  case '=':
    tok.kind = TOK_EQUAL;
    break;
  case '>':
    if (source_peek(lx->src) == '=') {
      (void)source_get(lx->src);
      tok.kind = TOK_GREQ;
    } else {
      tok.kind = TOK_GR;
    }
    break;
  case '<':
    if (source_peek(lx->src) == '=') {
      (void)source_get(lx->src);
      tok.kind = TOK_LEEQ;
    } else if (source_peek(lx->src) == '>') {
      (void)source_get(lx->src);
      tok.kind = TOK_NOTEQ;
    } else {
      tok.kind = TOK_LE;
    }
    break;
  case '(':
    tok.kind = TOK_LPAREN;
    break;
  case ')':
    tok.kind = TOK_RPAREN;
    break;
  case '[':
    tok.kind = TOK_LSQPAREN;
    break;
  case ']':
    tok.kind = TOK_RSQPAREN;
    break;
  case '.':
    tok.kind = TOK_DOT;
    break;
  case ',':
    tok.kind = TOK_COMMA;
    break;
  case ':':
    if (source_peek(lx->src) == '=') {
      (void)source_get(lx->src);
      tok.kind = TOK_ASSIGN;
    } else {
      tok.kind = TOK_COLON;
    }
    break;
  case ';':
    tok.kind = TOK_SEMI;
    break;
  default:
    /* 記号でない。kindはTOK_EOFのまま返り、呼び出しがわで報告 */
    return tok;
  }

  tok.text = start;
  tok.len = (size_t)(lx->src->text + lx->src->pos - start);
  return tok;
}

/*
 * 文字列を読み取る
 * 事前条件：トークンの先頭が'である
 */
static token_t lex_string(lexer_t *lx, token_t tok) {
  const char *start = lx->src->text + lx->src->pos; /* 開始記号 ' を含む綴りの先頭 */
  (void)source_get(lx->src);                        /* 開始記号を消化 */
  const char *body = lx->src->text + lx->src->pos;  /* 文字列の本体 */

  size_t len = 0;     /* ''を'に丸めた後の文字数 */
  size_t doubled = 0; /* ''の出現数 */
  bool closed = false;
  bool at_newline = false;

  for (;;) {
    int c = source_peek(lx->src);
    if (c == SOURCE_EOF || c == '\n') {
      at_newline = (c == '\n');
      break; /* closed はfalseのまま改行は消化しない */
    }
    const int line = lx->src->line;
    const int col = lx->src->col;
    (void)source_get(lx->src);
    if (!is_graphic(c)) {
      token_t bad = {0};
      bad.line = line;
      bad.col = col;
      report_bad_char(lx, &bad, c);
    }
    if (c != '\'') {
      len++;
      continue;
    }
    if (source_peek(lx->src) == '\'') {
      (void)source_get(lx->src);
      len++;
      doubled++;
      continue;
    }
    closed = true;
    break;
  }

  tok.kind = TOK_STRING;
  tok.text = start;
  tok.len = (size_t)(lx->src->text + lx->src->pos - start);

  if (!closed) {
    char msg[DIAG_MSG_MAX];
    (void)snprintf(msg, sizeof msg, "string is not closed before end of %s",
                   at_newline ? "line" : "file");
    diag_error(lx->diag, tok.line, tok.col, msg);
    tok.value.str.s = "";
    tok.value.str.len = 0;
    return tok;
  }

  if (doubled == 0) {
    tok.value.str.s = body;
    tok.value.str.len = len;
    return tok;
  }

  size_t raw = (size_t)(lx->src->text + lx->src->pos - 1 - body);
  char *buf = arena_alloc(lx->arena, len);
  size_t w = 0;
  for (size_t i = 0; i < raw; i++) {
    buf[w++] = body[i];
    if (body[i] == '\'') {
      i++;
    }
  }
  tok.value.str.s = buf;
  tok.value.str.len = len;
  return tok;
}

/*
 * 分離子(空白・タブ・改行・注釈)を読み飛ばす
 * 事後条件：次の１文字は分離子でないか終端である。
 */
static void skip_separators(lexer_t *lx) {
  for (;;) {
    int c = source_peek(lx->src);
    if (c == ' ' || c == '\t' || c == '\n') {
      (void)source_get(lx->src);
      continue;
    }
    if (c == '{') {
      /* {---}注釈を読み飛ばす */
      (void)source_get(lx->src);
      for (;;) {
        const int line = lx->src->line;
        const int col = lx->src->col;
        int k = source_get(lx->src);
        if (k == SOURCE_EOF || k == '}') {
          break;
        }
        if (!is_graphic(k)) {
          token_t bad = {0};
          bad.line = line;
          bad.col = col;
          report_bad_char(lx, &bad, k);
        }
      }
      continue;
    }
    if (c == '/' && source_peek2(lx->src) == '*') {
      /* 注釈を読み飛ばす */
      (void)source_get(lx->src);
      (void)source_get(lx->src);
      for (;;) {
        const int line = lx->src->line;
        const int col = lx->src->col;
        int k = source_get(lx->src);
        if (k == SOURCE_EOF) {
          break;
        }
        if (!is_graphic(k)) {
          token_t bad = {0};
          bad.line = line;
          bad.col = col;
          report_bad_char(lx, &bad, k);
        }
        if ((k == '*' && source_peek(lx->src) == '/')) {
          (void)source_get(lx->src);
          break;
        }
      }
      continue;
    }
    break; /* 分離子でないので、トークンの先頭 */
  }
}

/*
 * 次のトークンを１つ返す。
 */
token_t lexer_next(lexer_t *lx) {

  for (;;) {
    /* 分離子を読み飛ばす */
    skip_separators(lx);

    /* トークンの先頭の位置を控える */
    token_t tok = {0};
    tok.line = lx->src->line;
    tok.col = lx->src->col;

    int c = source_peek(lx->src);
    if (c == SOURCE_EOF) {
      return tok;
    }
    if (is_letter(c)) {
      return lex_name(lx, tok);
    }
    if (is_digit(c)) {
      return lex_number(lx, tok);
    }
    if (c == '\'') {
      return lex_string(lx, tok);
    }

    tok = lex_symbol(lx, tok);
    if (tok.kind != TOK_EOF) {
      return tok;
    }

    /* 記号でない文字である。lex_symbolが１文字消化済み。 */
    report_bad_char(lx, &tok, c);
  }
}
