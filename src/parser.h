#ifndef PARSER_H__
#define PARSER_H__

#include "array.h"

enum tn_expr_type {
	EXPR_NIL, EXPR_INT, EXPR_FLOAT, EXPR_STRING,
	EXPR_IDENT, EXPR_ASSN, EXPR_FN, EXPR_UOP,
	EXPR_BOP, EXPR_CALL, EXPR_IF, EXPR_ACCS,
	EXPR_PRNT, EXPR_DO, EXPR_LIST
};

struct tn_expr {
	enum tn_expr_type type;
	union {
		void *nil;
		int i;
		double d;
		const char *s;
		const char *id;
		struct tn_expr *expr;
		struct tn_expr_data_assn {
			const char *name;
			struct tn_expr *expr;
		} assn;
		struct tn_expr_data_fn {
			const char *name;
			char varargs;
			array_def (args, const char*);
			struct tn_expr *expr;
		} fn;
		struct tn_expr_data_uop {
			struct tn_expr *expr;
			int op;
		} uop;
		struct tn_expr_data_bop { 
			struct tn_expr *left, *right;
			int op;
		} bop;
		struct tn_expr_data_call {
			struct tn_expr *fn, *args;
		} call;
		struct tn_expr_data_ifs {
			struct tn_expr *cond, *t, *f;
		} ifs;
		struct tn_expr_data_accs {
			struct tn_expr *expr, *item;
		} accs;
	} data;
	struct tn_expr *next;
};

struct tn_token;
struct tn_expr *tn_parser_body (struct tn_token **tok);
void tn_parser_free (struct tn_expr *expr);

#endif
