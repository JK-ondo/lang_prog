#include "token.h"

#include <assert.h>
#include <string.h>

/* 同じリストをXの定義を変えて２回展開する。ズレる余地がない */
#define X(kind, symbol, spelling) spelling,
static const char * const kind_spellings[] = {TOKEN_KINDS(X)};
#undef X

#define X(kind, symbol, spelling) symbol,
static const char * const kind_symbols[] = {TOKEN_KINDS(X)};
#undef X

static_assert(sizeof kind_spellings / sizeof kind_spellings[0] == TOK_KIND_COUNT,
    "kind_spellings is different from token_kind_t");
static_assert(sizeof kind_symbols / sizeof kind_symbols[0] == TOK_KIND_COUNT,
    "kind_symbols is different from token_kind_t");

/* 予約語がenum上で連続していることに依存する。並び替えたらここが壊れる。 */
static_assert(TOK_WRITELN - TOK_AND + 1 == 28,
    "reserved word TOK_AND - TOK_WRITELN discontinuity");

/* 出現数の表に出す綴り。キーワードは"and"、記号は"+"、他は”NAME”など */
const char *token_kind_spelling(token_kind_t kind) {
    return kind_spellings[kind];
}

/* --dump-tokens に出す記号名。"AND" "PLUS" "ASSIGN" など */
const char *token_kind_symbol(token_kind_t kind) {
    return kind_symbols[kind];
}

/* 綴りが予約語ならその種類、違えば TOK_NAME を返す */
token_kind_t token_keyword_lookup(const char *s, size_t len) {
    for (token_kind_t k = TOK_AND; k <= TOK_WRITELN; k++) {
        const char * kw = kind_spellings[k];
        /* sはNUL終端してないので長さで比較 */
        if (strncmp(kw, s, len) == 0 && kw[len] == '\0') {return k;}
    }
    return TOK_NAME;
}
