#ifndef LEXER_H__
#define LEXER_H__

enum tn_token_type {
	TOK_ZERO, TOK_IMPT, TOK_PRNT, TOK_NIL, TOK_ELPS,
	TOK_FN, TOK_DO, TOK_EQ, TOK_NEQ, TOK_LTE,
	TOK_GTE, TOK_ANDL, TOK_ORL, TOK_CAT, TOK_LCAT,

	TOK_ASSN, TOK_LT, TOK_GT, TOK_LPAR, TOK_RPAR,
	TOK_LBRK, TOK_RBRK, TOK_COMM, TOK_ADD, TOK_SUB,
	TOK_MUL, TOK_DIV, TOK_MOD, TOK_COL, TOK_SCOL, TOK_QMRK,
	TOK_EXCL,

	TOK_IDENT, TOK_INT, TOK_FLOAT, TOK_STRING
};

struct tn_token {
	enum tn_token_type type;

	union {
		const char *s;
		int i;
		double d;
	} data;

	struct tn_token *next;
};

struct tn_token *tn_lexer_tokenize (char *src, struct tn_token **last);
struct tn_token *tn_lexer_tokenize_file (FILE *f);
void tn_lexer_free_tokens (struct tn_token *tok);

#endif
