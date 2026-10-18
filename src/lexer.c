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
    /* MPPL に無い文字。報告は未実装で、いまは終端として扱う */
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
    (void)source_get(lx->src);
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
 * 次のトークンを１つ返す。
 */
token_t lexer_next(lexer_t *lx) {
  /* 空白、タブ、改行を読み飛ばす */
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
        int k = source_get(lx->src);
        if (k == SOURCE_EOF || k == '}') {
          break;
        }
      }
      continue;
    }
    if (c == '/' && source_peek2(lx->src) == '*') {
      /* 注釈を読み飛ばす */
      (void)source_get(lx->src);
      (void)source_get(lx->src);
      for (;;) {
        int k = source_get(lx->src);
        if (k == SOURCE_EOF) {
          break;
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
  return lex_symbol(lx, tok);
}
