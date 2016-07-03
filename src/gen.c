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

	if (id)
		return id;
	else if (set) {
		id = ch->maxvar++;
		ch->vartree = tn_bst_insert (ch->vartree, name, (void*)(intptr_t)id);
		return id;
	}
	else
		return 0;
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
	uint32_t id = tn_gen_id_num (ch, name, 0);
	if (id != 0) {
		tn_gen_emit8 (ch, OP_PSHV);
		tn_gen_emit32 (ch, id);
		printf ("%.5s %s (%x)\n", "PSHV", name, id);
	}
	else if (!ch->name || strcmp (name, ch->name)) {
		// set an upvalue
		tn_gen_emit8 (ch, OP_UVAL);
		tn_gen_emit32 (ch, ch->upvals_num + 1);
		printf ("%.5s %s (%x)\n", "UVAL", name, ch->upvals_num + 1);
		array_add (ch->upvals, name);
	}
	else {
		tn_gen_emit8 (ch, OP_SELF);
		printf ("SELF\n");
	}
}

#define op(NAME) [TOK_##NAME] = OP_##NAME
static uint8_t op_opcodes[] = {
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
static void tn_gen_expr (struct tn_chunk *ch, struct tn_expr *ex)
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
			tn_gen_emit16 (ch, strlen (ex->data.s));

			for (i = 0; ex->data.s[i]; i++)
				tn_gen_emit8 (ch, ex->data.s[i]);
			printf ("PSHS \"%s\"\n", ex->data.s);
			break;
		case EXPR_IDENT:
			tn_gen_ident (ch, ex->data.id);
			break;
		case EXPR_ASSN:
			id = tn_gen_id_num (ch, ex->data.assn.name, 1);

			tn_gen_expr (ch, ex->data.assn.expr);
			tn_gen_emit8 (ch, OP_SET);
			tn_gen_emit32 (ch, id);
			printf ("%.5s %s (%i)\n", "SET", ex->data.assn.name, id);
			break;
		case EXPR_FN:
			printf (".FN  %i\n", nfunc++);
			array_add (ch->subch, tn_gen_compile (ex->data.fn.expr, ex->data.fn.name,
			                                   ex->data.fn.args, ex->data.fn.args_num));
			printf (".END %i\n", --nfunc);
			sub = ch->subch[ch->subch_num - 1];

			for (i = 0; i < sub->upvals_num; i++)
				tn_gen_ident (ch, sub->upvals[i]);

			tn_gen_emit8 (ch, OP_CLSR);
			tn_gen_emit16 (ch, ch->subch_num - 1);
			tn_gen_emit16 (ch, sub->upvals_num);
			printf ("%.5s %i %i\n", "CLSR", ch->subch_num - 1, sub->upvals_num);
			break;
		case EXPR_BOP:
			if (ex->data.bop.op == TOK_ANDL || ex->data.bop.op == TOK_ORL) {
				uint32_t j1, j2, out;

				tn_gen_expr (ch, ex->data.bop.left);
				tn_gen_emit8 (ch, ex->data.bop.op == TOK_ANDL ? OP_JZ : OP_JNZ);
				j1 = ch->pc;
				tn_gen_emit32 (ch, 0);
				printf ("%s\n", ex->data.bop.op == TOK_ANDL ? "JZ   .FALSE" : "JNZ  .TRUE");

				tn_gen_expr (ch, ex->data.bop.right);
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
				tn_gen_expr (ch, ex->data.bop.left);
				tn_gen_expr (ch, ex->data.bop.right);

				tn_gen_emit8 (ch, op_opcodes[ex->data.bop.op]);
				printf ("%.5s\n", bops[op_opcodes[ex->data.bop.op] - OP_ADD]);
			}
			break;
		case EXPR_CALL:
			it = ex->data.call.args;

			while (it) {
				tn_gen_expr (ch, it);
				it = it->next;
			}

			tn_gen_expr (ch, ex->data.call.fn);
			tn_gen_emit8 (ch, OP_CALL);
			printf ("CALL\n");
			break;
		case EXPR_IF: {
			uint32_t tskip, fskip; // we need to save positions for jump addresses

			tn_gen_expr (ch, ex->data.ifs.cond);
			tn_gen_emit8 (ch, OP_JZ);
			tskip = ch->pc;
			tn_gen_emit32 (ch, 0);
			printf ("%.5s .FALSE\n", "JZ");

			tn_gen_expr (ch, ex->data.ifs.t);
			tn_gen_emit8 (ch, OP_JMP);
			fskip = ch->pc;
			tn_gen_emit32 (ch, 0);
			printf ("%.5s .SKIP\n", "JMP");
			printf (".FALSE (%x)\n", ch->pc);
			tn_gen_emitpos (ch, ch->pc, tskip); // we jump here on false

			tn_gen_expr (ch, ex->data.ifs.f);
			printf (".SKIP (%x)\n", ch->pc);
			tn_gen_emitpos (ch, ch->pc, fskip); // we jump here after true
			break;
		}
		case EXPR_ACCS: {
			char *item;

			tn_gen_expr (ch, ex->data.accs.expr);
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
			tn_gen_expr (ch, ex->data.print);
			tn_gen_emit8 (ch, OP_PRNT);
			printf ("PRNT\n");
			break;
		default: break;
	}
}

struct tn_chunk *tn_gen_compile (struct tn_expr *ex, const char *name, const char **args, int argn)
{
	int i;
	struct tn_chunk *ret = malloc (sizeof (*ret));
	struct tn_expr *it = ex;

	if (!ret)
		return NULL;

	ret->pc = 0;
	ret->maxvar = 1;
	ret->name = name;
	array_init (ret->upvals);
	array_init (ret->subch);
	ret->vartree = NULL;

	// get ID numbers for all of the arguments
	for (i = 0; i < argn; i++)
		tn_gen_id_num (ret, args[i], 1);

	// generate code to pop the arguments off of the stack
	for (i = 1; i <= argn; i++) {
		tn_gen_emit8 (ret, OP_POP);
		tn_gen_emit32 (ret, i);
		printf ("%.5s %x\n", "POP", i);
	}

	while (it) {
		tn_gen_expr (ret, it);
		it = it->next;
	}

	tn_gen_emit8 (ret, OP_RET);

	return ret;
}
