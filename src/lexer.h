#ifndef LEXER_H__
#define LEXER_H__

typedef enum {
	TOK_ZERO, TOK_PRNT, TOK_NIL, TOK_FN, TOK_DO,
	TOK_EQ, TOK_NEQ, TOK_LTE, TOK_GTE, TOK_ANDL,
	TOK_ORL, TOK_CAT, TOK_LCAT,

	TOK_ASSN, TOK_LT, TOK_GT, TOK_LPAR, TOK_RPAR,
	TOK_COMM, TOK_ADD, TOK_SUB, TOK_MUL, TOK_DIV,
	TOK_MOD, TOK_COL, TOK_SCOL, TOK_QMRK,

	TOK_IDENT, TOK_INT, TOK_FLOAT, TOK_STRING
} TokenType;

typedef struct Builtin {
	const char *str;
	TokenType type;
} Builtin;

typedef struct Token {
	TokenType type;

	union {
		const char *s;
		int i;
		double d;
	} data;

	struct Token *next;
} Token;

#endif
