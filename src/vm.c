#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "opcode.h"
#include "parser.h"
#include "gen.h"
#include "vm.h"

Value *vm_value (enum val_type type, union val_data data)
{
	Value *ret = malloc (sizeof (*ret));

	if (!ret)
		return NULL;

	ret->type = type;
	ret->data = data;

	return ret;
}

uint16_t vm_read16 (Scope *sc)
{
	uint16_t i;

	i = sc->ch->code[sc->pc++];
	i |= sc->ch->code[sc->pc++] << 8;

	return i;
}

uint32_t vm_read32 (Scope *sc)
{
	uint32_t i;

	i = sc->ch->code[sc->pc++];
	i |= sc->ch->code[sc->pc++] << 8;
	i |= sc->ch->code[sc->pc++] << 16;
	i |= sc->ch->code[sc->pc++] << 24;

	return i;
}

uint64_t vm_read64 (Scope *sc)
{
	uint64_t i;

	i = sc->ch->code[sc->pc++];
	i |= sc->ch->code[sc->pc++] << 8;
	i |= sc->ch->code[sc->pc++] << 16;
	i |= sc->ch->code[sc->pc++] << 24;
	i |= (uint64_t)sc->ch->code[sc->pc++] << 32;
	i |= (uint64_t)sc->ch->code[sc->pc++] << 40;
	i |= (uint64_t)sc->ch->code[sc->pc++] << 48;
	i |= (uint64_t)sc->ch->code[sc->pc++] << 56;

	return i;
}

double vm_readdouble (Scope *sc)
{
	uint64_t u64 = vm_read64 (sc);
	double *d = (double*)&u64;
	return *d;
}

char *vm_readstring (Scope *sc)
{
	uint16_t len;
	char *ret;

	len = vm_read16 (sc);
	ret = strndup ((char*)sc->ch->code + sc->pc, len);
	if (!ret)
		return NULL;

	sc->pc += len;
	return ret;
}

static inline void vm_push (VM *vm, Value *val)
{
	vm->stack[vm->sp++] = val;
}

void vm_print (Value *val)
{
	Value *it;
	switch (val->type) {
		case VAL_INT:
			printf ("%i", val->data.i);
			break;
		case VAL_DBL:
			printf ("%g", val->data.d);
			break;
		case VAL_STR:
			printf ("%s", val->data.s);
			break;
		case VAL_PAIR:
			it = val;

			printf ("[");
			while (it) {
				vm_print (it->data.pair.a);
				it = it->data.pair.b;
				if (it)
					printf (", ");
			}
			printf ("]");
			break;
		case VAL_CLSR:
			printf ("closure 0x%lx", (uint64_t)val->data.cl);
			break;
		default: break;
	}
}

#define pop(DST) (DST = vm->stack[--vm->sp])
#define numop(OP) { \
	int v1i, v2i, v1d, v2d; \
	pop (v2); \
	pop (v1); \
	v1i = (v1->type == VAL_INT); \
	v2i = (v2->type == VAL_INT); \
	v1d = (v1->type == VAL_DBL); \
	v2d = (v2->type == VAL_DBL); \
	if (v1i && v2i) \
		vm_push (vm, vm_value (VAL_INT, ((union val_data) { .i = v1->data.i OP v2->data.i }))); \
	else if (v1d && v2d) \
		vm_push (vm, vm_value (VAL_DBL, ((union val_data) { .d = v1->data.d OP v2->data.d }))); \
	else if (v1i && v2d) \
		vm_push (vm, vm_value (VAL_DBL, ((union val_data) { .d = v1->data.i OP v2->data.d }))); \
	else if (v1d && v2i) \
		vm_push (vm, vm_value (VAL_DBL, ((union val_data) { .d = v1->data.d OP v2->data.i }))); \
} \
break

Closure *vm_closure (VM *vm, Scope *sc)
{
	Closure *ret = malloc (sizeof (*ret));
	uint16_t i, nargs;

	if (!ret)
		return NULL;

	ret->ch = sc->ch->subch[vm_read16 (sc)];
	array_init (ret->upvals);
	nargs = vm_read16 (sc);

	for (i = nargs; i > 0; i--)
		array_add_at (ret->upvals, vm->stack[--vm->sp], i - 1);

	return ret;
}

int vm_true (Value *v)
{
	switch (v->type) {
		case VAL_INT:
			return !!v->data.i;
		default: return 0;
	}
}

int vm_false (Value *v)
{
	return !vm_true (v);
}

Value *vm_cat (Value *a, Value *b)
{
	char *buf;

	if (a->type == VAL_STR && b->type == VAL_STR) {
		buf = malloc (strlen (a->data.s) + strlen (b->data.s) + 1);
		if (!buf)
			return NULL;

		sprintf (buf, "%s%s", a->data.s, b->data.s);
		return vm_value (VAL_STR, ((union val_data) { .s = buf }));
	}

	return NULL;
}

Value *vm_list_copy (Value *lst)
{
	if (!lst)
		return NULL;

	return vm_value (VAL_PAIR, ((union val_data) { .pair = { lst->data.pair.a, vm_list_copy (lst->data.pair.b) }}));
}

// list concatenation
Value *vm_lcat (Value *a, Value *b)
{
	Value *it;

	if (b->type != VAL_PAIR)
		b = vm_value (VAL_PAIR, ((union val_data) { .pair = { b, NULL }}));

	if (a->type == VAL_PAIR) {
		a = it = vm_list_copy (a);
		while (it->data.pair.b) // go to the end of this list
			it = it->data.pair.b;
		it->data.pair.b = b;

		return a;
	}
	else
		return vm_value (VAL_PAIR, ((union val_data) { .pair = { a, b }}));
}
		
void vm_dispatch (VM *vm, Chunk *ch, Closure *cl)
{
	Value *v1, *v2, *v3;
	Scope sc = { .pc = 0, .ch = ch };

	array_init (sc.vars);

	while (1) {
	//	printf ("op: %x\n", ch->code[sc.pc]);
		switch (ch->code[sc.pc++]) {
			case OP_NOP: break;
			case OP_ADD: numop (+);
			case OP_SUB: numop (-);
			case OP_MUL: numop (*);
			case OP_DIV: numop (/);
			case OP_MOD: // can't use numop here because we can't use doubles
				pop (v2);
				pop (v1);
				if (v1->type == VAL_INT && v2->type == VAL_INT)
					vm_push (vm,
					         vm_value (VAL_INT, ((union val_data) { .i = v1->data.i % v2->data.i })));
				break;
			case OP_EQ: numop (==);
			case OP_LT: numop (<);
			case OP_LTE: numop (<=);
			case OP_GT: numop (>);
			case OP_GTE: numop (>=);
			case OP_ANDL: // eventually use a boolean type for these, maybe
				pop (v2);
				pop (v1);
				vm_push (vm,
				         vm_value (VAL_INT, ((union val_data) { .i = vm_true (v1) && vm_true (v2) })));
				break;
			case OP_ORL:
				pop (v2);
				pop (v1);
				vm_push (vm,
				         vm_value (VAL_INT, ((union val_data) { .i = vm_true (v1) && vm_true (v2) })));
				break;
			case OP_CAT:
				pop (v2);
				pop (v1);
				vm_push (vm, vm_cat (v1, v2));
				break;
			case OP_LCAT:
				pop (v2);
				pop (v1);
				vm_push (vm, vm_lcat (v1, v2));
				break;
			case OP_PSHI:
				vm_push (vm, vm_value (VAL_INT, ((union val_data) { .i = vm_read32 (&sc) })));
				break;
			case OP_PSHD:
				vm_push (vm, vm_value (VAL_DBL, ((union val_data) { .d = vm_readdouble (&sc) })));
				break;
			case OP_PSHS:
				vm_push (vm, vm_value (VAL_STR, ((union val_data) { .s = vm_readstring (&sc) })));
				break;
			case OP_PSHV:
				vm_push (vm, sc.vars[vm_read32 (&sc) - 1]);
				break;
			case OP_UVAL:
				if (cl)
					vm_push (vm, cl->upvals[vm_read32 (&sc) - 1]);
				break;
			case OP_SET:
				array_add_at (sc.vars, vm->stack[vm->sp - 1], vm_read32 (&sc) - 1);
				break;
			case OP_POP:
				array_add_at (sc.vars, vm->stack[--vm->sp], vm_read32 (&sc) - 1);
				break;
			case OP_CLSR:
				vm_push (vm, vm_value (VAL_CLSR, ((union val_data) { .cl = vm_closure (vm, &sc) })));
				break;
			case OP_SELF:
				vm_push (vm, vm_value (VAL_CLSR, ((union val_data) { .cl = cl })));
				break;
			case OP_JMP:
				sc.pc = vm_read32 (&sc);
				break;
			case OP_JNZ:
				pop (v1);
				if (vm_true (v1))
					sc.pc = vm_read32 (&sc);
				else
					sc.pc += 4;
				break;
			case OP_JZ:
				pop (v1);
				if (vm_false (v1))
					sc.pc = vm_read32 (&sc);
				else
					sc.pc += 4;
				break;
			case OP_CALL:
				pop (v1);
				if (v1->type == VAL_CLSR) {
					//printf ("calling 0x%lx\n", (uint64_t)v1->data.cl);
					vm_dispatch (vm, v1->data.cl->ch, v1->data.cl);
				}
				break;
			case OP_PRNT:
				pop (v1);
				vm_print (v1);
				printf ("\n");
				break;
			case OP_RET:
			case OP_END: return;
			default: break;
		}
	}
}
