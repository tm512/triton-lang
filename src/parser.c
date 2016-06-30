#include <stdio.h>
#include <stdlib.h>

#include "error.h"
#include "parser.h"
#include "lexer.h"

static int parser_peek (Token **tok, TokenType type, int eat)
{
	if (*tok && (*tok)->type == type) {
		if (eat) *tok = (*tok)->next;
		return 1;
	}

	return 0;
}

static inline Expr *expralloc ()
{
	Expr *ret = malloc (sizeof (*ret));

	if (!ret) {
		error ("malloc failure\n");
		return NULL;
	}

	ret->type = EXPR_NIL;
	ret->next = NULL;

	return ret;
}

// recursively free an invalid syntax tree
static inline void exprfree (Expr *expr)
{
	if (!expr)
		return;

	switch (expr->type) {
		case EXPR_ASSN:
			exprfree (expr->data.assn.expr);
			break;
		case EXPR_FN:
			exprfree (expr->data.fn.expr);
			free (expr->data.fn.args);
			break;
		case EXPR_UOP:
			exprfree (expr->data.uop.expr);
			break;
		case EXPR_BOP:
			exprfree (expr->data.bop.left);
			exprfree (expr->data.bop.right);
			break;
		case EXPR_CALL:
			exprfree (expr->data.call.fn);
			exprfree (expr->data.call.args);
			break;
		case EXPR_IF:
			exprfree (expr->data.ifs.cond);
			exprfree (expr->data.ifs.t);
			exprfree (expr->data.ifs.f);
			break;
		case EXPR_ACCS:
			exprfree (expr->data.accs.expr);
			exprfree (expr->data.accs.item);
			break;
		default: break;
	}

	exprfree (expr->next);
	free (expr);
}

// wrappers for parser_peek
#define accept(TYPE) parser_peek (tok, TYPE, 1)
#define peek(TYPE) parser_peek (tok, TYPE, 0)
#define peeknext(TYPE) parser_peek (&((*tok)->next), TYPE, 0)

int parser_bop_check (Expr *expr)
{
	if (!expr)
		return 0;

	if (expr->type != EXPR_BOP)
		return 1;

	if (parser_bop_check (expr->data.bop.left) && parser_bop_check (expr->data.bop.right))
		return 1;
	else {
		exprfree (expr);
		return 0;
	}
}

Expr *parser_fn (Token**);
Expr *parser_if (Token **tok);
Expr *parser_negate (Token**);
Expr *parser_factor (Token **tok)
{
	// factor = (expr) | negate | ident [([args])] | int | float
	Token *prev = *tok;
	Expr *ret, *new, *tmp;

	ret = new = NULL;

	if (accept (TOK_SUB))
		return parser_negate (tok);

	if (accept (TOK_LPAR)) {
		ret = parser_if (tok);

		if (!accept (TOK_RPAR)) {
			error ("expected ')'\n");
			goto cleanup;
		}
	}
	else if (accept (TOK_FN)) {
		ret = parser_fn (tok);
		if (!ret) {
			error ("expected function definition\n");
			return NULL;
		}
	}
	else if (ret = expralloc (), accept (TOK_IDENT)) { // this keeps things concise, so whatever
		if (accept (TOK_ASSN)) {
			ret->type = EXPR_ASSN;
			ret->data.assn.name = prev->data.s;
			ret->data.assn.expr = parser_if (tok);

			if (!ret->data.assn.expr) {
				error ("expected expression after assignment operator\n");
				goto cleanup;
			}

			if (ret->data.assn.expr->type == EXPR_FN) // aaaaaa
				ret->data.assn.expr->data.fn.name = prev->data.s;
		}
		else {
			ret->type = EXPR_IDENT;
			ret->data.id = prev->data.s;
		}
	}
	else if (accept (TOK_INT)) {
		ret->type = EXPR_INT;
		ret->data.i = prev->data.i;
	}
	else if (accept (TOK_FLOAT)) {
		ret->type = EXPR_FLOAT;
		ret->data.d = prev->data.d;
	}
	else if (accept (TOK_STRING)) {
		ret->type = EXPR_STRING;
		ret->data.s = prev->data.s;
	}
	else if (accept (TOK_NIL)) {
		ret->type = EXPR_NIL;
		ret->data.nil = NULL;
	}
	else {
		error ("expected factor in expression\n");
		goto cleanup;
	}

	// list access
	while (accept (TOK_COL)) {
		new = expralloc ();

		new->type = EXPR_ACCS;
		new->data.accs.expr = ret;
		new->data.accs.item = parser_factor (tok);

		if (!new->data.accs.item) {
			error ("expected identifier after accessor\n");
			goto cleanup;
		}

		ret = new;
	}

	// function calls
	while (accept (TOK_LPAR)) {
		new = expralloc ();

		new->type = EXPR_CALL;
		new->data.call.fn = ret;
		new->data.call.args = NULL;

		while (!accept (TOK_RPAR)) { // build argument list
			tmp = parser_if (tok);

			if (!tmp) {
				error ("expected expression in argument list\n");
				goto cleanup;
			}

			tmp->next = new->data.call.args;
			new->data.call.args = tmp;
			accept (TOK_COMM);
		}

		ret = new;
	}

	return ret;

cleanup:
	exprfree (new ? new : ret);
	return NULL;
}

Expr *parser_negate (Token **tok)
{
	Expr *ret = expralloc ();

	ret->type = EXPR_UOP;
	ret->data.uop.expr = parser_factor (tok);
	ret->data.uop.op = TOK_SUB;

	return ret->data.uop.expr ? ret : NULL;
}

Expr *parser_term (Token **tok)
{
	// term = factor [*|/|% term]
	Token *op;
	Expr *new, *ret;

	ret = parser_factor (tok);

	while ((op = *tok) && (accept (TOK_MUL) || accept (TOK_DIV) || accept (TOK_MOD))) {
		new = expralloc ();

		new->type = EXPR_BOP;
		new->data.bop.left = ret;
		new->data.bop.right = parser_factor (tok);
		new->data.bop.op = op->type;

		ret = new;
	}

	return parser_bop_check (ret) ? ret : NULL;
}

Expr *parser_expr (Token **tok)
{
	// expr = term [+|- expr]
	Token *op;
	Expr *new, *ret;

	ret = parser_term (tok);

	while ((op = *tok) && (accept (TOK_ADD) || accept (TOK_SUB))) {
		new = expralloc ();

		new->type = EXPR_BOP;
		new->data.bop.left = ret;
		new->data.bop.right = parser_term (tok);
		new->data.bop.op = op->type;

		ret = new;
	}

	return parser_bop_check (ret) ? ret : NULL;
}

Expr *parser_cat (Token **tok)
{
	// eq = expr [==|!= eq]
	Token *op;
	Expr *new, *ret;

	ret = parser_expr (tok);

	op = *tok;
	if (accept (TOK_CAT) || accept (TOK_LCAT)) {
		new = expralloc ();

		new->type = EXPR_BOP;
		new->data.bop.left = ret;
		new->data.bop.right = parser_cat (tok);
		new->data.bop.op = op->type;

		ret = new;
	}

	return parser_bop_check (ret) ? ret : NULL;
}

Expr *parser_cmp (Token **tok)
{
	// eq = expr [==|!= eq]
	Token *op;
	Expr *new, *ret;

	ret = parser_cat (tok);

	while ((op = *tok) && (accept (TOK_EQ) || accept (TOK_NEQ) || accept (TOK_LT)
	                    || accept (TOK_LTE) || accept (TOK_GT) || accept (TOK_GTE))) {
		new = expralloc ();

		new->type = EXPR_BOP;
		new->data.bop.left = ret;
		new->data.bop.right = parser_cat (tok);
		new->data.bop.op = op->type;

		ret = new;
	}

	return parser_bop_check (ret) ? ret : NULL;
}

Expr *parser_bool (Token **tok)
{
	// eq = expr [==|!= eq]
	Token *op;
	Expr *new, *ret;

	ret = parser_cmp (tok);

	while ((op = *tok) && (accept (TOK_ANDL) || accept (TOK_ORL))) {
		new = expralloc ();

		new->type = EXPR_BOP;
		new->data.bop.left = ret;
		new->data.bop.right = parser_cmp (tok);
		new->data.bop.op = op->type;

		ret = new;
	}

	return parser_bop_check (ret) ? ret : NULL;
}

Expr *parser_body (Token**);
Expr *parser_fn (Token **tok)
{
	// fn = [name] (args) top ;
	struct expr_data_fn *fn;
	Token *prev;
	Expr *ret = expralloc ();

	prev = *tok;

	// fn name (args) == name = fn (args)
	if (accept (TOK_IDENT)) {
		ret->type = EXPR_ASSN;
		ret->data.assn.name = prev->data.s;
		ret->data.assn.expr = parser_fn (tok);
		if (!ret->data.assn.expr) {
			exprfree (ret);
			return NULL;
		}
		ret->data.assn.expr->data.fn.name = prev->data.s;
		return ret;
	}

	ret->type = EXPR_FN;
	fn = &ret->data.fn;

	if (!accept (TOK_LPAR)) {
		error ("expected argument list\n");
		exprfree (ret);
		return NULL;
	}

	// check if the argument list actually specifies any variables
	if (peek (TOK_IDENT))
		array_init (fn->args);
	else {
		fn->args = NULL;
		fn->args_num = 0;
		fn->args_max = 0;
	}

	prev = *tok;
	while (!accept (TOK_RPAR) && accept (TOK_IDENT)) { // build argument list
		// check if we need to resize the array
		array_add (fn->args, prev->data.s);
		accept (TOK_COMM);
		prev = *tok;
	}

	if (!prev || prev->type != TOK_RPAR) {
		error ("unexpected end to argument list\n");
		exprfree (ret);
		return NULL;
	}

	fn->expr = parser_body (tok);

	if (!fn->expr) {
		error ("expected function body\n");
		exprfree (ret);
		return NULL;
	}

	if (0)
	{
		int i;
		Expr *it = fn->expr;

		printf ("name: %s\nargs: ", fn->name);
		for (i = 0; i < fn->args_num; i++)
			printf ("%s ", fn->args[i]);
		printf ("\n");

		// count body expressions
		printf ("expressions:\n");
		while (it) {
			print_expr (it);
			printf ("\n");
			it = it->next;
		}
		printf ("\n");
	}

	return ret;
}

Expr *parser_if (Token **tok)
{
	Expr *ret, *new;

	ret = parser_bool (tok);

	if (!ret)
		return NULL;

	if (accept (TOK_QMRK)) {
		new = expralloc ();

		new->type = EXPR_IF;
		new->data.ifs.cond = ret;
		new->data.ifs.t = parser_if (tok);
		new->data.ifs.f = parser_if (tok);

		if (!new->data.ifs.t || !new->data.ifs.f) {
			exprfree (new);
			return NULL;
		}

		ret = new;
	}

	return ret;
}

// top-level expression, used for source files, functions, do-statements
// unlike basically everything else, this returns a list of Expr
Expr *parser_body (Token **tok)
{
	// top = fn | assn | expr [top]
	Token *prev;
	Expr *ret, *last, *new;

	ret = last = NULL;

	while (*tok && !accept (TOK_SCOL)) {
		if (accept (TOK_PRNT)) {
			new = expralloc ();

			new->type = EXPR_PRNT;
			new->data.print = parser_if (tok);
		}
		else
			new = parser_if (tok);

		if (!new) {
			exprfree (ret); // free what we have
			return NULL;
		}

		if (last) {
			last->next = new;
			last = new;
		}
		else
			ret = last = new;
	}

	return ret;
}
