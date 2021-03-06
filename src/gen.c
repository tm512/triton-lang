#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

#include "error.h"
#include "hash.h"
#include "lexer.h"
#include "parser.h"
#include "opcode.h"
#include "gen.h"
#include "vm.h"

// turn string identifiers into numbers
static uint32_t tn_gen_id_num (struct tn_chunk *ch, const char *name, int set)
{
	// the BST associates void* with a key
	// I don't want to allocate a bunch more stuff, so we're just storing the int in the pointer :/
	uint32_t id = (uint32_t)tn_hash_search (ch->vars->hash, name);

	if (id || !set)
		return id;
	else {
		id = ++ch->vars->maxid;
		if (tn_hash_insert (ch->vars->hash, name, (void*)(intptr_t)id))
			return 0;
		return id;
	}
}

static void tn_gen_emit8 (struct tn_chunk *ch, uint8_t n)
{
	if (ch->pc >= ch->codelen) {
		uint8_t *code = ch->code;

		ch->code = realloc (ch->code, ch->codelen *= 2);
		if (!ch->code) {
			tn_error ("realloc failed\n");
			ch->code = code; // pretend like nothing happened I guess
			return;
		}
	}

	ch->code[ch->pc++] = n;
}

static void tn_gen_emit16 (struct tn_chunk *ch, uint16_t n)
{
	tn_gen_emit8 (ch, n & 0xff);
	tn_gen_emit8 (ch, (n & 0xff00) >> 8);
}

static void tn_gen_emit32 (struct tn_chunk *ch, uint32_t n)
{
	tn_gen_emit16 (ch, n & 0xffff);
	tn_gen_emit16 (ch, (n & 0xffff0000) >> 16);
}

static void tn_gen_emit64 (struct tn_chunk *ch, uint64_t n)
{
	tn_gen_emit32 (ch, n & 0xffffffff);
	tn_gen_emit32 (ch, (n & 0xffffffff00000000) >> 32);
}

static void tn_gen_emitdouble (struct tn_chunk *ch, double n)
{
	uint64_t *u64 = (uint64_t*)&n;
	tn_gen_emit64 (ch, *u64);
}

static void tn_gen_emitstring (struct tn_chunk *ch, const char *s)
{
	int i;

	tn_gen_emit16 (ch, strlen (s));

	for (i = 0; s[i]; i++)
		tn_gen_emit8 (ch, s[i]);
}

static void tn_gen_emitpos (struct tn_chunk *ch, uint32_t n, uint32_t pos)
{
	ch->code[pos++] = n & 0xff;
	ch->code[pos++] = (n & 0xff00) >> 8;
	ch->code[pos++] = (n & 0xff0000) >> 16;
	ch->code[pos++] = (n & 0xff000000) >> 24;
}

static void tn_gen_ident (struct tn_chunk *ch, const char *name)
{
	struct tn_chunk *it = ch;
	uint32_t id, frame = 0;

	while (it) {
		id = tn_gen_id_num (it, name, 0);
		if (id != 0) {
			tn_gen_emit8 (ch, OP_PSHV);
			tn_gen_emit16 (ch, frame); // how many frames up we need to go to find a binding
			tn_gen_emit32 (ch, id);
			return;
		}

		frame++;
		it = it->next;
	}

	// this is either a global or unbound
	id = tn_gen_id_num (ch, name, 1);
	tn_gen_emit8 (ch, OP_GLOB);
	tn_gen_emitstring (ch, name);
	tn_gen_emit8 (ch, OP_SET);
	tn_gen_emit32 (ch, id);
}

#define op(NAME) [TOK_##NAME] = OP_##NAME
static uint8_t uop_opcodes[] = {
	[TOK_SUB] = OP_NEG,
	[TOK_EXCL] = OP_NOT
};

static uint8_t bop_opcodes[] = {
	op (ADD),
	op (SUB),
	op (MUL),
	op (DIV),
	op (MOD),
	op (EQ),
	op (NEQ),
	op (LT),
	op (LTE),
	op (GT),
	op (GTE),
	op (ANDL),
	op (ORL),
	op (CAT),
	op (LCAT)
};
#undef op

static void tn_gen_expr (struct tn_chunk *ch, struct tn_expr *ex, int final)
{
	uint32_t id;
	struct tn_expr *it;

	switch (ex->type) {
		case EXPR_NIL:
			tn_gen_emit8 (ch, OP_NIL);
			break;
		case EXPR_INT:
			tn_gen_emit8 (ch, OP_PSHI);
			tn_gen_emit32 (ch, ex->data.i);
			break;
		case EXPR_FLOAT:
			tn_gen_emit8 (ch, OP_PSHD);
			tn_gen_emitdouble (ch, ex->data.d);
			break;
		case EXPR_STRING:
			tn_gen_emit8 (ch, OP_PSHS);
			tn_gen_emitstring (ch, ex->data.s);
			break;
		case EXPR_IDENT:
			tn_gen_ident (ch, ex->data.id);
			break;
		case EXPR_ASSN:
			id = tn_gen_id_num (ch, ex->data.assn.name, 1);

			tn_gen_expr (ch, ex->data.assn.expr, 0);
			tn_gen_emit8 (ch, OP_SET);
			tn_gen_emit32 (ch, id);
			break;
		case EXPR_FN: {
			struct tn_chunk *new;

			new = tn_gen_compile (ex->data.fn.expr, &ex->data.fn, ch, NULL);
			array_add (ch->subch, new);

			tn_gen_emit8 (ch, OP_CLSR);
			tn_gen_emit16 (ch, ch->subch_num - 1);
			break;
		}
		case EXPR_UOP:
			tn_gen_expr (ch, ex->data.uop.expr, 0);
			tn_gen_emit8 (ch, uop_opcodes[ex->data.uop.op]);
			break;
		case EXPR_BOP:
			// && and || require some flow control
			if (ex->data.bop.op == TOK_ANDL || ex->data.bop.op == TOK_ORL) {
				uint32_t j1, j2, out;

				tn_gen_expr (ch, ex->data.bop.left, 0);
				tn_gen_emit8 (ch, ex->data.bop.op == TOK_ANDL ? OP_JZ : OP_JNZ);
				j1 = ch->pc;
				tn_gen_emit32 (ch, 0);

				tn_gen_expr (ch, ex->data.bop.right, 0);
				tn_gen_emit8 (ch, ex->data.bop.op == TOK_ANDL ? OP_JZ : OP_JNZ);
				j2 = ch->pc;
				tn_gen_emit32 (ch, 0);

				// results
				tn_gen_emit8 (ch, OP_PSHI);
				tn_gen_emit32 (ch, ex->data.bop.op == TOK_ANDL);
				tn_gen_emit8 (ch, OP_JMP);
				out = ch->pc;
				tn_gen_emit32 (ch, 0);

				// false/true
				tn_gen_emitpos (ch, ch->pc, j1);
				tn_gen_emitpos (ch, ch->pc, j2);

				tn_gen_emit8 (ch, OP_PSHI);
				tn_gen_emit32 (ch, ex->data.bop.op == TOK_ORL);

				// out
				tn_gen_emitpos (ch, ch->pc, out);
			}
			else {
				tn_gen_expr (ch, ex->data.bop.left, 0);
				tn_gen_expr (ch, ex->data.bop.right, 0);

				tn_gen_emit8 (ch, bop_opcodes[ex->data.bop.op]);
			}
			break;
		case EXPR_CALL: {
			int nargs = 0;

			it = ex->data.call.args;

			while (it) {
				tn_gen_expr (ch, it, 0);
				nargs++;
				it = it->next;
			}

			tn_gen_expr (ch, ex->data.call.fn, 0);
			tn_gen_emit8 (ch, final ? OP_TCAL : OP_CALL);
			tn_gen_emit32 (ch, nargs);
			break;
		}
		case EXPR_IF: {
			uint32_t tskip, fskip; // we need to save positions for jump addresses

			tn_gen_expr (ch, ex->data.ifs.cond, 0);
			tn_gen_emit8 (ch, OP_JZ);
			tskip = ch->pc;
			tn_gen_emit32 (ch, 0);

			tn_gen_expr (ch, ex->data.ifs.t, final); // pass through "final" here
			tn_gen_emit8 (ch, OP_JMP);
			fskip = ch->pc;
			tn_gen_emit32 (ch, 0);
			tn_gen_emitpos (ch, ch->pc, tskip); // we jump here on false

			if (ex->data.ifs.f)
				tn_gen_expr (ch, ex->data.ifs.f, final);
			else
				tn_gen_emit8 (ch, OP_NIL);

			tn_gen_emitpos (ch, ch->pc, fskip); // we jump here after true
			break;
		}
		case EXPR_ACCS: {
			tn_gen_expr (ch, ex->data.accs.expr, 0);

			tn_gen_emit8 (ch, OP_ACCS);
			tn_gen_emitstring (ch, ex->data.accs.item);
			break;
		}
		case EXPR_DO:
			it = ex->data.expr;

			while (it) {
				tn_gen_expr (ch, it, 0);
				it = it->next;

				if (it) // discard this value, we don't need it on the stack
					tn_gen_emit8 (ch, OP_DROP);
			}

			break;
		case EXPR_LIST:
			tn_gen_emit8 (ch, OP_LSTS);

			it = ex->data.expr;
			while (it) {
				tn_gen_expr (ch, it, 0);
				it = it->next;
			}

			tn_gen_emit8 (ch, OP_LSTE);
			break;
		case EXPR_IMPT: {
			const char *name = ex->data.s;

			tn_gen_emit8 (ch, OP_IMPT);
			tn_gen_emitstring (ch, name);

			// now bind it to a value
			id = tn_gen_id_num (ch, name, 1);
			tn_gen_emit8 (ch, OP_SET);
			tn_gen_emit32 (ch, id);
			break;
		}
		default: break;
	}
}

struct tn_chunk *tn_gen_compile (struct tn_expr *ex, struct tn_expr_data_fn *fn,
                                 struct tn_chunk *next, struct tn_chunk_vars *vars)
{
	int i;
	struct tn_chunk *ret = malloc (sizeof (*ret));
	struct tn_expr *it = ex;

	if (!ret)
		goto error;

	ret->codelen = 16;
	ret->code = malloc (ret->codelen);

	if (!ret->code)
		goto error;

	ret->pc = 0;
	ret->name = fn ? fn->name : NULL;
	array_init (ret->subch);
	ret->path = NULL;
	if (vars)
		ret->vars = vars;
	else {
		ret->vars = malloc (sizeof (*ret->vars));

		if (!ret->vars) {
			free (ret->code);
			goto error;
		}

		ret->vars->maxid = 0;
		ret->vars->hash = tn_hash_new (8);
	}

	ret->next = next;

	if (fn) {
		// get ID numbers for all of the arguments
		for (i = 0; i < fn->args_num; i++)
			tn_gen_id_num (ret, fn->args[i], 1);

		tn_gen_emit8 (ret, OP_ARGS);
		tn_gen_emit8 (ret, fn->varargs);
		tn_gen_emit32 (ret, fn->args_num);
	}

	while (it) {
		tn_gen_expr (ret, it, !it->next);
		it = it->next;

		if (it) // discard this value, we don't need it on the stack
			tn_gen_emit8 (ret, OP_DROP);
	}

	tn_gen_emit8 (ret, OP_RET);
	return ret;

error:
	tn_error ("malloc failed\n");
	free (ret);
	return NULL;
}
