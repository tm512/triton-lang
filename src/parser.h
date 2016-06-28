#ifndef PARSER_H__
#define PARSER_H__

#include "array.h"

typedef enum ExprType {
	EXPR_INT, EXPR_FLOAT, EXPR_STRING, EXPR_IDENT,
	EXPR_ASSN, EXPR_FN, EXPR_UOP, EXPR_BOP,
	EXPR_CALL, EXPR_IF, EXPR_PRNT
} ExprType;

struct ExprList;
typedef struct Expr {
	ExprType type;
	union {
		int i;
		double d;
		const char *s;
		const char *id;
		struct Expr *print;
		struct expr_data_assn {
			const char *name;
			struct Expr *expr;
		} assn;
		struct expr_data_fn {
			const char *name;
			array_def (args, const char*);
			struct Expr *expr;
		} fn;
		struct expr_data_uop {
			struct Expr *expr;
			int op;
		} uop;
		struct expr_data_bop { 
			struct Expr *left, *right;
			int op;
		} bop;
		struct expr_data_call {
			struct Expr *fn, *args;
		} call;
		struct expr_data_ifs {
			struct Expr *cond, *t, *f;
		} ifs;
	} data;
	struct Expr *next;
} Expr;

struct Token;
Expr *parser_body (struct Token**);

#endif
