#include <stdio.h>
#include <stdlib.h>

#include "error.h"
#include "parser.h"
#include "lexer.h"

static int tn_parser_peek (struct tn_token **tok, enum tn_token_type type, int eat)
{
	if (*tok && (*tok)->type == type) {
		if (eat) *tok = (*tok)->next;
		return 1;
	}

	return 0;
}

static inline struct tn_expr *tn_parser_alloc ()
{
	struct tn_expr *ret = malloc (sizeof (*ret));

	if (!ret) {
		tn_error ("malloc failure\n");
		return NULL;
	}

	ret->type = EXPR_NIL;
	ret->next = NULL;

	return ret;
}

// recursively free an invalid syntax tree
void tn_parser_free (struct tn_expr *expr)
{
	if (!expr)
		return;

	switch (expr->type) {
		case EXPR_ASSN:
			tn_parser_free (expr->data.assn.expr);
			break;
		case EXPR_FN:
			tn_parser_free (expr->data.fn.expr);
			free (expr->data.fn.args);
			break;
		case EXPR_UOP:
			tn_parser_free (expr->data.uop.expr);
			break;
		case EXPR_BOP:
			tn_parser_free (expr->data.bop.left);
			tn_parser_free (expr->data.bop.right);
			break;
		case EXPR_CALL:
			tn_parser_free (expr->data.call.fn);
			tn_parser_free (expr->data.call.args);
			break;
		case EXPR_IF:
			tn_parser_free (expr->data.ifs.cond);
			tn_parser_free (expr->data.ifs.t);
			tn_parser_free (expr->data.ifs.f);
			break;
		case EXPR_ACCS:
			tn_parser_free (expr->data.accs.expr);
			break;
		case EXPR_DO:
			tn_parser_free (expr->data.expr);
			break;
		default: break;
	}

	tn_parser_free (expr->next);
	free (expr);
}

// wrappers for tn_parser_peek
#define accept(TYPE) tn_parser_peek (tok, TYPE, 1)
#define peek(TYPE) tn_parser_peek (tok, TYPE, 0)
#define peeknext(TYPE) tn_parser_peek (&((*tok)->next), TYPE, 0)

int tn_parser_bop_check (struct tn_expr *expr)
{
	if (!expr)
		return 0;

	if (expr->type != EXPR_BOP)
		return 1;

	if (tn_parser_bop_check (expr->data.bop.left) && tn_parser_bop_check (expr->data.bop.right))
		return 1;
	else {
		tn_parser_free (expr);
		return 0;
	}
}

struct tn_expr *tn_parser_fn (struct tn_token**);
struct tn_expr *tn_parser_if (struct tn_token **);
struct tn_expr *tn_parser_uop (struct tn_token**);
struct tn_expr *tn_parser_factor (struct tn_token **tok)
{
	// factor = (expr) | negate | ident [([args])] | int | float
	struct tn_token *prev = *tok;
	struct tn_expr *ret, *new;

	ret = new = NULL;

	if (accept (TOK_LPAR)) {
		ret = tn_parser_if (tok);

		if (!accept (TOK_RPAR)) {
			tn_error ("expected ')'\n");
			goto cleanup;
		}
	}
	else if (accept (TOK_FN)) {
		ret = tn_parser_fn (tok);
		if (!ret) {
			tn_error ("expected function definition\n");
			return NULL;
		}
	}
	else if (ret = tn_parser_alloc (), accept (TOK_IDENT)) { // this keeps things concise, so whatever
		if (accept (TOK_ASSN)) {
			ret->type = EXPR_ASSN;
			ret->data.assn.name = prev->data.s;
			ret->data.assn.expr = tn_parser_if (tok);

			if (!ret->data.assn.expr) {
				tn_error ("expected expression after assignment operator\n");
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
	else if (accept (TOK_DO)) {
		ret->type = EXPR_DO;
		ret->data.expr = tn_parser_body (tok);

		if (!ret->data.expr) {
			tn_error ("expected body after \"do\"\n");
			return NULL;
		}
	}
	else if (accept (TOK_LBRK)) {
		ret->type = EXPR_LIST;
		ret->data.expr = NULL;

		while (!accept (TOK_RBRK)) {
			new = tn_parser_if (tok);

			if (!new) {
				tn_error ("expected expression in list\n");
				goto cleanup;
			}

			new->next = ret->data.expr;
			ret->data.expr = new;

			if (!accept (TOK_COMM) && !peek (TOK_RBRK)) {
				tn_error ("unexpected end to list\n");
				goto cleanup;
			}
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
		tn_error ("expected factor in expression\n");
		goto cleanup;
	}

	// list access
	while (accept (TOK_COL)) {
		new = tn_parser_alloc ();

		new->type = EXPR_ACCS;
		new->data.accs.expr = ret;

		prev = *tok;
		if (!accept (TOK_IDENT)) {
			tn_error ("expected identifier after accessor\n");
			goto cleanup;
		}

		new->data.accs.item = prev->data.s;
		ret = new;
	}

	return ret;

cleanup:
	tn_parser_free (new ? new : ret);
	return NULL;
}

struct tn_expr *tn_parser_call (struct tn_token **tok)
{
	struct tn_expr *new, *ret, *args;

	ret = tn_parser_factor (tok);
	new = NULL;

	while (accept (TOK_LPAR)) {
		new = tn_parser_alloc ();

		new->type = EXPR_CALL;
		new->data.call.fn = ret;
		new->data.call.args = NULL;

		while (!accept (TOK_RPAR)) { // build argument list
			args = tn_parser_if (tok);

			if (!args) {
				tn_error ("expected expression in argument list\n");
				goto cleanup;
			}

			args->next = new->data.call.args;
			new->data.call.args = args;

			if (!accept (TOK_COMM) && !peek (TOK_RPAR)) {
				tn_error ("unexpected end to argument list\n");
				goto cleanup;
			}
		}

		ret = new;
	}

	return ret;

cleanup:
	tn_parser_free (new ? new : ret);
	return NULL;
}

struct tn_expr *tn_parser_uop (struct tn_token **tok)
{
	struct tn_token *op;
	struct tn_expr *ret;

	op = *tok;
	if (accept (TOK_SUB) || accept (TOK_EXCL)) {
		ret = tn_parser_alloc ();

		ret->type = EXPR_UOP;
		ret->data.uop.expr = tn_parser_uop (tok);
		ret->data.uop.op = op->type;

		return ret->data.uop.expr ? ret : NULL;
	}
	else
		return tn_parser_call (tok);
}

struct tn_expr *tn_parser_term (struct tn_token **tok)
{
	// term = factor [*|/|% term]
	struct tn_token *op;
	struct tn_expr *new, *ret;

	ret = tn_parser_uop (tok);

	while ((op = *tok) && (accept (TOK_MUL) || accept (TOK_DIV) || accept (TOK_MOD))) {
		new = tn_parser_alloc ();

		new->type = EXPR_BOP;
		new->data.bop.left = ret;
		new->data.bop.right = tn_parser_uop (tok);
		new->data.bop.op = op->type;

		ret = new;
	}

	return tn_parser_bop_check (ret) ? ret : NULL;
}

struct tn_expr *tn_parser_expr (struct tn_token **tok)
{
	// expr = term [+|- expr]
	struct tn_token *op;
	struct tn_expr *new, *ret;

	ret = tn_parser_term (tok);

	while ((op = *tok) && (accept (TOK_ADD) || accept (TOK_SUB))) {
		new = tn_parser_alloc ();

		new->type = EXPR_BOP;
		new->data.bop.left = ret;
		new->data.bop.right = tn_parser_term (tok);
		new->data.bop.op = op->type;

		ret = new;
	}

	return tn_parser_bop_check (ret) ? ret : NULL;
}

struct tn_expr *tn_parser_cat (struct tn_token **tok)
{
	// eq = expr [==|!= eq]
	struct tn_token *op;
	struct tn_expr *new, *ret;

	ret = tn_parser_expr (tok);

	op = *tok;
	if (accept (TOK_CAT) || accept (TOK_LCAT)) {
		new = tn_parser_alloc ();

		new->type = EXPR_BOP;
		new->data.bop.left = ret;
		new->data.bop.right = tn_parser_cat (tok);
		new->data.bop.op = op->type;

		ret = new;
	}

	return tn_parser_bop_check (ret) ? ret : NULL;
}

struct tn_expr *tn_parser_cmp (struct tn_token **tok)
{
	// eq = expr [==|!= eq]
	struct tn_token *op;
	struct tn_expr *new, *ret;

	ret = tn_parser_cat (tok);

	while ((op = *tok) && (accept (TOK_EQ) || accept (TOK_NEQ) || accept (TOK_LT)
	                    || accept (TOK_LTE) || accept (TOK_GT) || accept (TOK_GTE))) {
		new = tn_parser_alloc ();

		new->type = EXPR_BOP;
		new->data.bop.left = ret;
		new->data.bop.right = tn_parser_cat (tok);
		new->data.bop.op = op->type;

		ret = new;
	}

	return tn_parser_bop_check (ret) ? ret : NULL;
}

struct tn_expr *tn_parser_bool (struct tn_token **tok)
{
	// eq = expr [==|!= eq]
	struct tn_token *op;
	struct tn_expr *new, *ret;

	ret = tn_parser_cmp (tok);

	while ((op = *tok) && (accept (TOK_ANDL) || accept (TOK_ORL))) {
		new = tn_parser_alloc ();

		new->type = EXPR_BOP;
		new->data.bop.left = ret;
		new->data.bop.right = tn_parser_cmp (tok);
		new->data.bop.op = op->type;

		ret = new;
	}

	return tn_parser_bop_check (ret) ? ret : NULL;
}

struct tn_expr *tn_parser_body (struct tn_token**);
struct tn_expr *tn_parser_fn (struct tn_token **tok)
{
	// fn = [name] (args) top ;
	struct tn_expr_data_fn *fn;
	struct tn_token *prev;
	struct tn_expr *ret = tn_parser_alloc ();

	prev = *tok;

	// fn name (args) == name = fn (args)
	if (accept (TOK_IDENT)) {
		ret->type = EXPR_ASSN;
		ret->data.assn.name = prev->data.s;
		ret->data.assn.expr = tn_parser_fn (tok);
		if (!ret->data.assn.expr) {
			tn_parser_free (ret);
			return NULL;
		}
		ret->data.assn.expr->data.fn.name = prev->data.s;
		return ret;
	}

	ret->type = EXPR_FN;
	fn = &ret->data.fn;

	if (!accept (TOK_LPAR)) {
		tn_error ("expected argument list\n");
		tn_parser_free (ret);
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

	fn->varargs = 0;

	prev = *tok;
	while (!accept (TOK_RPAR) && (accept (TOK_IDENT) || accept (TOK_LBRK))) { // build argument list
		if (prev->type == TOK_LBRK) {
			fn->varargs = 1;
			prev = *tok;
			if (!accept (TOK_IDENT) || !accept (TOK_RBRK)) {
				tn_error ("expected variadic argument\n");
				tn_parser_free (ret);
				return NULL;
			}
		}

		array_add (fn->args, prev->data.s);

		if (!accept (TOK_COMM) && !peek (TOK_RPAR)) {
			tn_error ("expected argument list\n");
			tn_parser_free (ret);
			return NULL;
		}

		prev = *tok;
	}

	if (!prev || prev->type != TOK_RPAR) {
		tn_error ("unexpected end to argument list\n");
		tn_parser_free (ret);
		return NULL;
	}

	fn->expr = tn_parser_body (tok);

	if (!fn->expr) {
		tn_error ("expected function body\n");
		tn_parser_free (ret);
		return NULL;
	}

	return ret;
}

struct tn_expr *tn_parser_if (struct tn_token **tok)
{
	struct tn_expr *ret;

	if (accept (TOK_IF)) {
		ret = tn_parser_alloc ();

		ret->type = EXPR_IF;
		ret->data.ifs.cond = tn_parser_bool (tok);
		ret->data.ifs.t = tn_parser_if (tok);

		if (accept (TOK_ELSE))
			ret->data.ifs.f = tn_parser_if (tok);
		else
			ret->data.ifs.f = NULL;

		return ret;
	}
	else
		return tn_parser_bool (tok);
}

// top-level expression, used for source files, functions, do-statements
// unlike basically everything else, this returns a list of struct tn_expr
struct tn_expr *tn_parser_body (struct tn_token **tok)
{
	// top = fn | assn | expr [top]
	struct tn_token *prev;
	struct tn_expr *ret, *last, *new;

	ret = last = NULL;

	while (*tok && !accept (TOK_SCOL) && !peek (TOK_COMM) && !peek (TOK_RPAR)) {
		if (accept (TOK_IMPT)) {
			new = tn_parser_alloc ();
			prev = *tok;

			new->type = EXPR_IMPT;
			if (accept (TOK_STRING))
				new->data.s = prev->data.s;
			else {
				tn_error ("expected identifier after import\n");
				tn_parser_free (ret);
				return NULL;
			}
		}
		else
			new = tn_parser_if (tok);

		if (!new) {
			tn_parser_free (ret); // free what we have
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
