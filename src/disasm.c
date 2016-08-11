#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "opcode.h"
#include "vm.h"

enum {
	OA_8 = 1,
	OA_16,
	OA_32,
	OA_DBL,
	OA_STR
};

static struct tn_disasm_opinfo {
	const char *name;
	unsigned int opands[4];
} opinfo[] = {
	[OP_NOP] =	{ "NOP",	{ 0 } },
	[OP_ADD] =	{ "ADD",	{ 0 } },
	[OP_SUB] =	{ "SUB",	{ 0 } },
	[OP_MUL] =	{ "MUL",	{ 0 } },
	[OP_DIV] =	{ "DIV",	{ 0 } },
	[OP_MOD] =	{ "MOD",	{ 0 } },
	[OP_EQ] =	{ "EQ",		{ 0 } },
	[OP_NEQ] =	{ "NEQ",	{ 0 } },
	[OP_LT] =	{ "LT",		{ 0 } },
	[OP_LTE] =	{ "LTE",	{ 0 } },
	[OP_GT] =	{ "GT",		{ 0 } },
	[OP_GTE] =	{ "GTE",	{ 0 } },
	[OP_ANDL] =	{ "ANDL",	{ 0 } },
	[OP_ORL] =	{ "ORL",	{ 0 } },
	[OP_CAT] =	{ "CAT",	{ 0 } },
	[OP_LCAT] =	{ "LCAT",	{ 0 } },
	[OP_PSHI] =	{ "PSHI",	{ OA_32, 0 } },
	[OP_PSHD] =	{ "PSHD",	{ OA_DBL, 0 } },
	[OP_PSHS] =	{ "PSHS",	{ OA_STR, 0 } },
	[OP_PSHV] =	{ "PSHV",	{ OA_16, OA_32, 0 } },
	[OP_SET] =	{ "SET",	{ OA_32, 0 } },
	[OP_DROP] =	{ "DROP",	{ 0 } },
	[OP_CLSR] =	{ "CLSR",	{ OA_16, 0 } },
	[OP_SELF] =	{ "SELF",	{ 0 } },
	[OP_NIL] =	{ "NIL",	{ 0 } },
	[OP_GLOB] =	{ "GLOB",	{ OA_STR, 0 } },
	[OP_ARGS] =	{ "ARGS",	{ OA_8, OA_32, 0 } },
	[OP_JMP] =	{ "JMP",	{ OA_32, 0 } },
	[OP_JNZ] =	{ "JNZ",	{ OA_32, 0 } },
	[OP_JZ] =	{ "JZ",		{ OA_32, 0 } },
	[OP_CALL] =	{ "CALL",	{ OA_32, 0 } },
	[OP_TCAL] =	{ "TCAL",	{ OA_32, 0 } },
	[OP_RET] =	{ "RET",	{ 0 } },
	[OP_ACCS] =	{ "ACCS",	{ OA_STR, 0 } },
	[OP_IDX] =	{ "IDX",	{ 0 } },
	[OP_LSTS] =	{ "LSTS",	{ 0 } },
	[OP_LSTE] =	{ "LSTE",	{ 0 } },
	[OP_MACC] =	{ "MACC",	{ OA_32, 0 } },
	[OP_NEG] =	{ "NEG",	{ 0 } },
	[OP_NOT] =	{ "NOT",	{ 0 } },
	[OP_IMPT] =	{ "IMPT",	{ OA_16, 0 } },
	[OP_PRNT] =	{ "PRNT",	{ 0 } },
	[OP_END] =	{ "END",	{ 0 } }
};

static inline uint8_t tn_disasm_read8 (struct tn_chunk *ch)
{
	return ch->code[ch->pc++];
}

static uint16_t tn_disasm_read16 (struct tn_chunk *ch)
{
	uint16_t i;

	i = ch->code[ch->pc++];
	i |= ch->code[ch->pc++] << 8;

	return i;
}

static uint32_t tn_disasm_read32 (struct tn_chunk *ch)
{
	uint32_t i;

	i = ch->code[ch->pc++];
	i |= ch->code[ch->pc++] << 8;
	i |= ch->code[ch->pc++] << 16;
	i |= ch->code[ch->pc++] << 24;

	return i;
}

static uint64_t tn_disasm_read64 (struct tn_chunk *ch)
{
	uint64_t i;

	i = ch->code[ch->pc++];
	i |= ch->code[ch->pc++] << 8;
	i |= ch->code[ch->pc++] << 16;
	i |= ch->code[ch->pc++] << 24;
	i |= (uint64_t)ch->code[ch->pc++] << 32;
	i |= (uint64_t)ch->code[ch->pc++] << 40;
	i |= (uint64_t)ch->code[ch->pc++] << 48;
	i |= (uint64_t)ch->code[ch->pc++] << 56;

	return i;
}

static double tn_disasm_readdouble (struct tn_chunk *ch)
{
	uint64_t u64 = tn_disasm_read64 (ch);
	double *d = (double*)&u64;
	return *d;
}

static char *tn_disasm_readstring (struct tn_chunk *ch)
{
	uint16_t len;
	char *ret;

	len = tn_disasm_read16 (ch);
	ret = strndup ((char*)ch->code + ch->pc, len);
	if (!ret)
		return NULL;

	ch->pc += len;
	return ret;
}

void tn_disasm (struct tn_chunk *ch)
{
	int i;
	struct tn_disasm_opinfo *op;

	printf ("chunk %x (%s):\n", ch, ch->name);

	ch->pc = 0;

	while (1) {
		printf ("%05x ", ch->pc);

		op = &opinfo[ch->code[ch->pc++]];
		printf ("%-5s", op->name);

		for (i = 0; op->opands[i]; i++) {
			switch (op->opands[i]) {
				case OA_8:
					printf ("0x%x ", tn_disasm_read8 (ch));
					break;
				case OA_16:
					printf ("0x%x ", tn_disasm_read16 (ch));
					break;
				case OA_32:
					printf ("0x%x ", tn_disasm_read32 (ch));
					break;
				case OA_DBL:
					printf ("%g ", tn_disasm_readdouble (ch));
					break;
				case OA_STR:
					printf ("%s ", tn_disasm_readstring (ch));
					break;
				default: break;
			}
		}

		printf ("\n");
		if (op == &opinfo[OP_RET])
			break;
	}

	for (i = 0; i < ch->subch_num; i++)
		tn_disasm (ch->subch[i]);
}
