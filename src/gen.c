#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bst.h"
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
	uint32_t id = (uint32_t)tn_bst_find (ch->vartree, name);

	if (id || !set)
		return id;
	else {
		id = (uint32_t)tn_bst_find (ch->vartree, "") + 1; // the blank key holds what the highest id is
		ch->vartree = tn_bst_insert (ch->vartree, name, (void*)(intptr_t)id);
		ch->vartree = tn_bst_insert (ch->vartree, "", (void*)(intptr_t)id);
		return id;
	}
}

static void tn_gen_emit8 (struct tn_chunk *ch, uint8_t n)
{
	ch->code[ch->pc++] = n;
}

static void tn_gen_emit16 (struct tn_chunk *ch, uint16_t n)
{
	ch->code[ch->pc++] = n & 0xff;
	ch->code[ch->pc++] = (n & 0xff00) >> 8;
}

static void tn_gen_emit32 (struct tn_chunk *ch, uint32_t n)
{
	ch->code[ch->pc++] = n & 0xff;
	ch->code[ch->pc++] = (n & 0xff00) >> 8;
	ch->code[ch->pc++] = (n & 0xff0000) >> 16;
	ch->code[ch->pc++] = (n & 0xff000000) >> 24;
}

static void tn_gen_emit64 (struct tn_chunk *ch, uint64_t n)
{
	ch->code[ch->pc++] = n & 0xff;
	ch->code[ch->pc++] = (n & 0xff00) >> 8;
	ch->code[ch->pc++] = (n & 0xff0000) >> 16;
	ch->code[ch->pc++] = (n & 0xff000000) >> 24;
	ch->code[ch->pc++] = (n & 0xff00000000) >> 32;
	ch->code[ch->pc++] = (n & 0xff0000000000) >> 40;
	ch->code[ch->pc++] = (n & 0xff000000000000) >> 48;
	ch->code[ch->pc++] = (n & 0xff00000000000000) >> 56;
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

#define printf(...) (0) // silence
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
			printf ("PSHV %u %u (%s)\n", frame, id, name);
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
	printf ("GLOB %s\nSET  %s (%i)\n", name, name, id);
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

static int nfunc = 0;
static void tn_gen_expr (struct tn_chunk *ch, struct tn_expr *ex, int final)
{
	uint32_t i, id;
	struct tn_expr *it;
	struct tn_chunk *sub;
	static const char *bops[] = { "ADD", "SUB", "MUL", "DIV", "MOD",
	                              "EQ", "NEQ", "LT", "LTE", "GT",
	                              "GTE", "ANDL", "ORL", "CAT",
	                              "LCAT" };

	switch (ex->type) {
		case EXPR_NIL:
			tn_gen_emit8 (ch, OP_NIL);
			break;
		case EXPR_INT:
			tn_gen_emit8 (ch, OP_PSHI);
			tn_gen_emit32 (ch, ex->data.i);
			printf ("%.5s %i\n", "PSHI", ex->data.i);
			break;
		case EXPR_FLOAT:
			tn_gen_emit8 (ch, OP_PSHD);
			tn_gen_emitdouble (ch, ex->data.d);
			printf ("%.5s %g\n", "PSHD", ex->data.d);
			break;
		case EXPR_STRING:
			tn_gen_emit8 (ch, OP_PSHS);
			tn_gen_emitstring (ch, ex->data.s);
			printf ("PSHS \"%s\"\n", ex->data.s);
			break;
		case EXPR_IDENT:
			tn_gen_ident (ch, ex->data.id);
			break;
		case EXPR_ASSN:
			id = tn_gen_id_num (ch, ex->data.assn.name, 1);

			tn_gen_expr (ch, ex->data.assn.expr, 0);
			tn_gen_emit8 (ch, OP_SET);
			tn_gen_emit32 (ch, id);
			printf ("%.5s %s (%i)\n", "SET", ex->data.assn.name, id);
			break;
		case EXPR_FN: {
			struct tn_chunk *new;

			printf (".FN  %i\n", nfunc++);
			new = tn_gen_compile (ex->data.fn.expr, &ex->data.fn, ch, NULL);
			printf (".END %i\n", --nfunc);
			array_add (ch->subch, new);
			sub = ch->subch[ch->subch_num - 1];

			tn_gen_emit8 (ch, OP_CLSR);
			tn_gen_emit16 (ch, ch->subch_num - 1);
			printf ("%.5s %i\n", "CLSR", ch->subch_num - 1);
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
				printf ("%s\n", ex->data.bop.op == TOK_ANDL ? "JZ   .FALSE" : "JNZ  .TRUE");

				tn_gen_expr (ch, ex->data.bop.right, 0);
				tn_gen_emit8 (ch, ex->data.bop.op == TOK_ANDL ? OP_JZ : OP_JNZ);
				j2 = ch->pc;
				tn_gen_emit32 (ch, 0);
				printf ("%s\n", ex->data.bop.op == TOK_ANDL ? "JZ   .FALSE" : "JNZ  .TRUE");

				// results
				tn_gen_emit8 (ch, OP_PSHI);
				tn_gen_emit32 (ch, ex->data.bop.op == TOK_ANDL);
				printf ("PSHI %i\n", ex->data.bop.op == TOK_ANDL);
				tn_gen_emit8 (ch, OP_JMP);
				out = ch->pc;
				tn_gen_emit32 (ch, 0);
				printf ("JMP  .OUT\n");

				// false/true
				tn_gen_emitpos (ch, ch->pc, j1);
				tn_gen_emitpos (ch, ch->pc, j2);
				printf (".%s (%x)\n", ex->data.bop.op == TOK_ANDL ? "FALSE" : "TRUE", ch->pc);

				tn_gen_emit8 (ch, OP_PSHI);
				tn_gen_emit32 (ch, ex->data.bop.op == TOK_ORL);
				printf ("PSHI %i\n", ex->data.bop.op == TOK_ORL);

				// out
				tn_gen_emitpos (ch, ch->pc, out);
				printf (".OUT (%x)\n", ch->pc);
			}
			else {
				tn_gen_expr (ch, ex->data.bop.left, 0);
				tn_gen_expr (ch, ex->data.bop.right, 0);

				tn_gen_emit8 (ch, bop_opcodes[ex->data.bop.op]);
				printf ("%.5s\n", bops[bop_opcodes[ex->data.bop.op] - OP_ADD]);
			}
			break;
		case EXPR_CALL: {
			int nargs = 0;
			struct tn_expr_data_fn *fn;

			it = ex->data.call.args;

			while (it) {
				tn_gen_expr (ch, it, 0);
				nargs++;
				it = it->next;
			}

			tn_gen_expr (ch, ex->data.call.fn, 0);
			tn_gen_emit8 (ch, final ? OP_TCAL : OP_CALL);
			tn_gen_emit32 (ch, nargs);
			printf ("CALL %i\n", nargs);
			break;
		}
		case EXPR_IF: {
			uint32_t tskip, fskip; // we need to save positions for jump addresses

			tn_gen_expr (ch, ex->data.ifs.cond, 0);
			tn_gen_emit8 (ch, OP_JZ);
			tskip = ch->pc;
			tn_gen_emit32 (ch, 0);
			printf ("%.5s .FALSE\n", "JZ");

			tn_gen_expr (ch, ex->data.ifs.t, final); // pass through "final" here
			tn_gen_emit8 (ch, OP_JMP);
			fskip = ch->pc;
			tn_gen_emit32 (ch, 0);
			printf ("%.5s .SKIP\n", "JMP");
			printf (".FALSE (%x)\n", ch->pc);
			tn_gen_emitpos (ch, ch->pc, tskip); // we jump here on false

			tn_gen_expr (ch, ex->data.ifs.f, final);
			printf (".SKIP (%x)\n", ch->pc);
			tn_gen_emitpos (ch, ch->pc, fskip); // we jump here after true
			break;
		}
		case EXPR_ACCS: {
			const char *item;

			tn_gen_expr (ch, ex->data.accs.expr, 0);
			if (ex->data.accs.item->type == EXPR_IDENT) {
				item = ex->data.accs.item->data.s;
				if (item[0] == 'h') {
					tn_gen_emit8 (ch, OP_HEAD);
					printf ("HEAD\n");
				}
				else if (item[0] == 't') {
					tn_gen_emit8 (ch, OP_TAIL);
					printf ("TAIL\n");
				}
			}
			break;
		}
		case EXPR_PRNT:
			tn_gen_expr (ch, ex->data.print, 0);
			tn_gen_emit8 (ch, OP_PRNT);
			printf ("PRNT\n");
			break;
		default: break;
	}
}

struct tn_chunk *tn_gen_compile (struct tn_expr *ex, struct tn_expr_data_fn *fn,
                                 struct tn_chunk *next, struct tn_bst *vartree)
{
	int i;
	struct tn_chunk *ret = malloc (sizeof (*ret));
	struct tn_expr *it = ex;

	if (!ret)
		return NULL;

	ret->pc = 0;
	ret->name = fn ? fn->name : NULL;
	array_init (ret->subch);
	ret->vartree = vartree;
	ret->next = next;

	if (fn) {
		// get ID numbers for all of the arguments
		for (i = 0; i < fn->args_num; i++)
			tn_gen_id_num (ret, fn->args[i], 1);

		tn_gen_emit8 (ret, OP_ARGS);
		tn_gen_emit8 (ret, fn->varargs);
		tn_gen_emit32 (ret, fn->args_num);
		printf ("ARGS %u %i\n", fn->varargs, fn->args_num);
	}

	while (it) {
		tn_gen_expr (ret, it, !it->next);
		it = it->next;

		if (it) { // discard this value, we don't need it on the stack
			tn_gen_emit8 (ret, OP_POP);
			tn_gen_emit32 (ret, 0);
		}
	}

	tn_gen_emit8 (ret, OP_RET);
	printf ("RET\n");

	return ret;
}
