#include <stdio.h>
#include <stdlib.h>

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

// wrapper for the above
#define accept(TYPE) parser_peek (tok, TYPE, 1)
#define peek(TYPE) parser_peek (tok, TYPE, 0)
#define peeknext(TYPE) parser_peek (&((*tok)->next), TYPE, 0)

Expr *parser_fn (Token**);
Expr *parser_if (Token **tok);
Expr *parser_negate (Token**);
Expr *parser_factor (Token **tok)
{
	// factor = (expr) | negate | ident [([args])] | int | float
	Token *prev = *tok;
	Expr *ret, *new, *tmp;

	if (accept (TOK_SUB))
		return parser_negate (tok);

	ret = malloc (sizeof (*ret));
	if (!ret)
		return NULL;

	if (accept (TOK_LPAR)) {
		ret = parser_if (tok);

		if (!accept (TOK_RPAR)) // missing )
			return NULL;
	}
	else if (accept (TOK_FN))
		ret = parser_fn (tok);
	else if (accept (TOK_IDENT)) {
		if (accept (TOK_ASSN)) {
			ret->type = EXPR_ASSN;
			ret->data.assn.name = prev->data.s;
			ret->data.assn.expr = parser_if (tok);
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
	else
		return NULL;

	// list access
	while (accept (TOK_COL)) {
		new = malloc (sizeof (*new));
		if (!new)
			return NULL;

		new->type = EXPR_ACCS;
		new->data.accs.expr = ret;
		new->data.accs.item = parser_factor (tok);

		ret = new;
	}

	// function calls
	while (accept (TOK_LPAR)) {
		new = malloc (sizeof (*new));
		if (!new)
			return NULL;

		new->type = EXPR_CALL;
		new->data.call.fn = ret;
		new->data.call.args = NULL;
		while (!accept (TOK_RPAR)) { // build argument list
			tmp = parser_if (tok);
			tmp->next = new->data.call.args;
			new->data.call.args = tmp;
			accept (TOK_COMM);
		}

		ret = new;
	}

	return ret;
}

Expr *parser_negate (Token **tok)
{
	Expr *ret = malloc (sizeof (*ret));

	if (!ret)
		return NULL;

	ret->type = EXPR_UOP;
	ret->data.uop.expr = parser_factor (tok);
	ret->data.uop.op = TOK_SUB;

	return ret;
}

Expr *parser_term (Token **tok)
{
	// term = factor [*|/|% term]
	Token *op;
	Expr *new, *ret;

	ret = parser_factor (tok);

	while ((op = *tok) && (accept (TOK_MUL) || accept (TOK_DIV) || accept (TOK_MOD))) {
		new = malloc (sizeof (*new));
		if (!new)
			return NULL;

		new->type = EXPR_BOP;
		new->data.bop.left = ret;
		new->data.bop.right = parser_factor (tok);
		new->data.bop.op = op->type;

		ret = new;
	}

	return ret;
}

Expr *parser_expr (Token **tok)
{
	// expr = term [+|- expr]
	Token *op;
	Expr *new, *ret;

	ret = parser_term (tok);

	while ((op = *tok) && (accept (TOK_ADD) || accept (TOK_SUB))) {
		new = malloc (sizeof (*new));
		if (!new)
			return NULL;

		new->type = EXPR_BOP;
		new->data.bop.left = ret;
		new->data.bop.right = parser_term (tok);
		new->data.bop.op = op->type;

		ret = new;
	}

	return ret;
}

Expr *parser_cat (Token **tok)
{
	// eq = expr [==|!= eq]
	Token *op;
	Expr *new, *ret;

	ret = parser_expr (tok);

	op = *tok;
	if (accept (TOK_CAT) || accept (TOK_LCAT)) {
		new = malloc (sizeof (*new));
		if (!new)
			return NULL;

		new->type = EXPR_BOP;
		new->data.bop.left = ret;
		new->data.bop.right = parser_cat (tok);
		new->data.bop.op = op->type;

		ret = new;
	}

	return ret;
}

Expr *parser_cmp (Token **tok)
{
	// eq = expr [==|!= eq]
	Token *op;
	Expr *new, *ret;

	ret = parser_cat (tok);

	while ((op = *tok) && (accept (TOK_EQ) || accept (TOK_NEQ) || accept (TOK_LT)
	                    || accept (TOK_LTE) || accept (TOK_GT) || accept (TOK_GTE))) {
		new = malloc (sizeof (*new));
		if (!new)
			return NULL;

		new->type = EXPR_BOP;
		new->data.bop.left = ret;
		new->data.bop.right = parser_cat (tok);
		new->data.bop.op = op->type;

		ret = new;
	}

	return ret;
}

Expr *parser_bool (Token **tok)
{
	// eq = expr [==|!= eq]
	Token *op;
	Expr *new, *ret;

	ret = parser_cmp (tok);

	while ((op = *tok) && (accept (TOK_ANDL) || accept (TOK_ORL))) {
		new = malloc (sizeof (*new));
		if (!new)
			return NULL;

		new->type = EXPR_BOP;
		new->data.bop.left = ret;
		new->data.bop.right = parser_cmp (tok);
		new->data.bop.op = op->type;

		ret = new;
	}

	return ret;
}

void print_expr (Expr *ex)
{
	printf ("( ");
	switch (ex->type) {
		case EXPR_BOP:
			print_expr (ex->data.bop.left);
			printf ("op:%i ", ex->data.bop.op);
			print_expr (ex->data.bop.right);
			break;
		case EXPR_IDENT:
			printf ("id:%s ", ex->data.id);
			break;
		case EXPR_INT:
			printf ("%i ", ex->data.i);
			break;
		default: break;
	}
	printf (") ");
}

Expr *parser_body (Token**);
Expr *parser_fn (Token **tok)
{
	// fn = [name] (args) top ;
	struct expr_data_fn *fn;
	Token *prev;
	Expr *ret = malloc (sizeof (*ret));

	if (!ret)
		return NULL;

	prev = *tok;

	// fn name (args) == name = fn (args)
	if (accept (TOK_IDENT)) {
		ret->type = EXPR_ASSN;
		ret->data.assn.name = prev->data.s;
		ret->data.assn.expr = parser_fn (tok);
		ret->data.assn.expr->data.fn.name = prev->data.s;
		return ret;
	}

	ret->type = EXPR_FN;
	fn = &ret->data.fn;

	if (!accept (TOK_LPAR))
		return NULL; // no argument list

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

	fn->expr = parser_body (tok);

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

	if (accept (TOK_QMRK)) {
		new = malloc (sizeof (*new));
		if (!new)
			return NULL;

		new->type = EXPR_IF;
		new->data.ifs.cond = ret;
		new->data.ifs.t = parser_if (tok);
		new->data.ifs.f = parser_if (tok);

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
			new = malloc (sizeof (*new));
			if (!new)
				return NULL;

			new->type = EXPR_PRNT;
			new->data.print = parser_if (tok);
		}
		else
			new = parser_if (tok);

		if (last) {
			last->next = new;
			last = new;
		}
		else
			ret = last = new;
	}

	return ret;
}
