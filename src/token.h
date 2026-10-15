#ifndef MPPLC_TOKEN_H
#define MPPLC_TOKEN_H

#include <stddef.h>

/*
 * トークンの種類と２つの名前を１つのリストで保持する。
 *  X(列挙子、記号名、綴)
 *    記号名ー --dump-tokensに出す名前（外部仕様§4.2）
 *    綴り　ー 出現数の表に出す字句（課題１の提出物）
 * この順序が出現数の表の出力順になる。並べ替えると出力の並びが変わる。
 */

#define TOKEN_KINDS(X)                          \
  X(TOK_EOF,        "EOF",      "EOF")          \
  /* 予約後２８ */                              \
  X(TOK_AND,        "AND",      "and")          \
  X(TOK_ARRAY,      "ARRAY",    "array" )       \
  X(TOK_BEGIN,      "BEGIN",    "begin")        \
  X(TOK_BOOLEAN,    "BOOLEAN",  "boolean")      \
  X(TOK_BREAK,      "BREAK",    "break")        \
  X(TOK_CALL,       "CALL",     "call")         \
  X(TOK_CHAR,       "CHAR",     "char")         \
  X(TOK_DIV,        "DIV",      "div")          \
  X(TOK_DO,         "DO",       "do")           \
  X(TOK_ELSE,       "ELSE",     "else")         \
  X(TOK_END,        "END",      "end")          \
  X(TOK_FALSE,      "FALSE",    "false")        \
  X(TOK_IF,         "IF",       "if")           \
  X(TOK_INTEGER,    "INTEGER",  "integer")      \
  X(TOK_NOT,        "NOT",      "not")          \
  X(TOK_OF,         "OF",       "of")           \
  X(TOK_OR,         "OR",       "or")           \
  X(TOK_PROCEDURE,  "PROCEDURE","procedure")    \
  X(TOK_PROGRAM,    "PROGRAM",  "program")      \
  X(TOK_READ,       "READ",     "read")         \
  X(TOK_READLN,     "READLN",   "readln")       \
  X(TOK_RETURN,     "RETURN",   "return")       \
  X(TOK_THEN,       "THEN",     "then")         \
  X(TOK_TRUE,       "TRUE",     "true")         \
  X(TOK_VAR,        "VAR",      "var")          \
  X(TOK_WHILE,      "WHILE",    "while")        \
  X(TOK_WRITE,      "WRITE",    "write")        \
  X(TOK_WRITELN,    "WRITELN",  "writeln")      \
  /* 記号18 */                                  \
  X(TOK_PLUS,       "PLUS",     "+")            \
  X(TOK_MINUS,      "MINUS",    "-")            \
  X(TOK_STAR,       "STAR",     "*")            \
  X(TOK_EQUAL,      "EQUAL",    "=")            \
  X(TOK_NOTEQ,      "NOTEQ",    "<>")           \
  X(TOK_LE,         "LE",       "<")            \
  X(TOK_LEEQ,       "LEEQ",     "<=")           \
  X(TOK_GR,         "GR",       ">")            \
  X(TOK_GREQ,       "GREQ",     ">=")           \
  X(TOK_LPAREN,     "LPAREN",   "(")            \
  X(TOK_RPAREN,     "RPAREN",   ")")            \
  X(TOK_LSQPAREN,   "LSQPAREN", "[")            \
  X(TOK_RSQPAREN,   "RSQPAREN", "]")            \
  X(TOK_ASSIGN,     "ASSIGN",   ":=")           \
  X(TOK_DOT,        "DOT",      ".")            \
  X(TOK_COMMA,      "COMMA",    ",")            \
  X(TOK_COLON,      "COLON",    ":")            \
  X(TOK_SEMI,       "SEMI",     ";")            \
  /* その他　３ */                              \
  X(TOK_NAME,       "NAME",     "NAME")         \
  X(TOK_NUMBER,     "NUMBER",   "NUMBER")       \
  X(TOK_STRING,     "STRING",   "STRING")

#define X(kind, symbol, spelling) kind,
typedef enum {
    TOKEN_KINDS(X) TOK_KIND_COUNT
}token_kind_t;
#undef X

typedef struct {
    token_kind_t kind;
    int line, col;      /* トークンの先頭の位置。１起点 */
    const char * text;  /* 綴り。sourceのバッファを指す。NUL終端されてない。 */
    size_t len;         /* 綴りの長さ。*/
    union {
        unsigned number;    /* TOK_NUMBER: 0 ~ 32768 */
        struct {
            const char *s;
            size_t len;     /* ''を１文字に戻した後の長さ。 */
        } str;          /* TOK_STRING */
    } value;
} token_t;

/* 出現数の表に出す綴り。キーワードは"and"、記号は"+"、他は”NAME”など */
const char *token_kind_spelling(token_kind_t kind);

/* --dump-tokens に出す記号名。"AND" "PLUS" "ASSIGN" など */
const char *token_kind_symbol(token_kind_t kind);

/* 綴りが予約語ならその種類、違えば TOK_NAME を返す */
token_kind_t token_keyword_lookup(const char *s, size_t len);

#endif /* MPPLC_TOKEN_H */
