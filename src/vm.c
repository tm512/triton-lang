#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "opcode.h"
#include "parser.h"
#include "gen.h"
#include "vm.h"
#include "gc.h"

Value nil = { VAL_NIL, ((union val_data) { 0 }) };

Value *vm_value (VM *vm, enum val_type type, union val_data data)
{
	Value *ret = gc_alloc (vm->gc);

	if (!ret)
		return NULL;

	ret->type = type;
	ret->data = data;

	return ret;
}

uint16_t vm_read16 (VM *vm)
{
	uint16_t i;

	i = vm->sc->ch->code[vm->sc->pc++];
	i |= vm->sc->ch->code[vm->sc->pc++] << 8;

	return i;
}

uint32_t vm_read32 (VM *vm)
{
	uint32_t i;

	i = vm->sc->ch->code[vm->sc->pc++];
	i |= vm->sc->ch->code[vm->sc->pc++] << 8;
	i |= vm->sc->ch->code[vm->sc->pc++] << 16;
	i |= vm->sc->ch->code[vm->sc->pc++] << 24;

	return i;
}

uint64_t vm_read64 (VM *vm)
{
	uint64_t i;

	i = vm->sc->ch->code[vm->sc->pc++];
	i |= vm->sc->ch->code[vm->sc->pc++] << 8;
	i |= vm->sc->ch->code[vm->sc->pc++] << 16;
	i |= vm->sc->ch->code[vm->sc->pc++] << 24;
	i |= (uint64_t)vm->sc->ch->code[vm->sc->pc++] << 32;
	i |= (uint64_t)vm->sc->ch->code[vm->sc->pc++] << 40;
	i |= (uint64_t)vm->sc->ch->code[vm->sc->pc++] << 48;
	i |= (uint64_t)vm->sc->ch->code[vm->sc->pc++] << 56;

	return i;
}

double vm_readdouble (VM *vm)
{
	uint64_t u64 = vm_read64 (vm);
	double *d = (double*)&u64;
	return *d;
}

char *vm_readstring (VM *vm)
{
	uint16_t len;
	char *ret;

	len = vm_read16 (vm);
	ret = strndup ((char*)vm->sc->ch->code + vm->sc->pc, len);
	if (!ret)
		return NULL;

	vm->sc->pc += len;
	return ret;
}

static inline void vm_push (VM *vm, Value *val)
{
	if (vm->sp == vm->ss) {
		vm->ss *= 2;
		vm->stack = realloc (vm->stack, vm->ss * sizeof (Value*));

		if (!vm->stack) {
			error ("realloc failure\n");
			return;
		}
	}

	vm->stack[vm->sp++] = val;

	/* this is my "hack" to keep the GC behaved
	   if the GC only scans up to the stack pointer, it could miss
	   operands that were popped, but are still in scope */
	vm->stack[vm->sp] = NULL; // GC should terminate here
}

void vm_print (Value *val)
{
	Value *it;
	switch (val->type) {
		case VAL_NIL:
			printf ("nil");
			break;
		case VAL_INT:
			printf ("%i", val->data.i);
			break;
		case VAL_DBL:
			printf ("%g", val->data.d);
			break;
		case VAL_STR:
			printf ("\"%s\"", val->data.s);
			break;
		case VAL_PAIR:
			it = val;

			printf ("[");
			while (it != &nil) {
				vm_print (it->data.pair.a);
				it = it->data.pair.b;
				if (it != &nil)
					printf (" ");
			}
			printf ("]");
			break;
		case VAL_CLSR:
			printf ("closure 0x%lx", (uint64_t)val->data.cl);
			break;
		default: break;
	}
}

#define pop() (vm->stack[--vm->sp])
#define numop(OP) { \
	int v1i, v2i, v1d, v2d; \
	v2 = pop (); \
	v1 = pop (); \
	v1i = (v1->type == VAL_INT); \
	v2i = (v2->type == VAL_INT); \
	v1d = (v1->type == VAL_DBL); \
	v2d = (v2->type == VAL_DBL); \
	if (v1i && v2i) \
		vm_push (vm, vm_value (vm, VAL_INT, ((union val_data) { .i = v1->data.i OP v2->data.i }))); \
	else if (v1d && v2d) \
		vm_push (vm, vm_value (vm, VAL_DBL, ((union val_data) { .d = v1->data.d OP v2->data.d }))); \
	else if (v1i && v2d) \
		vm_push (vm, vm_value (vm, VAL_DBL, ((union val_data) { .d = v1->data.i OP v2->data.d }))); \
	else if (v1d && v2i) \
		vm_push (vm, vm_value (vm, VAL_DBL, ((union val_data) { .d = v1->data.d OP v2->data.i }))); \
} \
break

Closure *vm_closure (VM *vm)
{
	Closure *ret = malloc (sizeof (*ret));
	uint16_t i, nargs;

	if (!ret)
		return NULL;

	ret->ch = vm->sc->ch->subch[vm_read16 (vm)];
	array_init (ret->upvals);
	nargs = vm_read16 (vm);

	for (i = nargs; i > 0; i--)
		array_add_at (ret->upvals, vm->stack[--vm->sp], i - 1);

	return ret;
}

int vm_true (Value *v)
{
	if (!v)
		return 0;

	switch (v->type) {
		case VAL_NIL: return 0;
		case VAL_INT: return !!v->data.i;
		case VAL_PAIR: return v->data.pair.a != &nil;
		default: return 0;
	}
}

int vm_false (Value *v)
{
	return !vm_true (v);
}

Value *vm_cat (VM *vm, Value *a, Value *b)
{
	char *buf;

	if (a->type == VAL_STR && b->type == VAL_STR) {
		buf = malloc (strlen (a->data.s) + strlen (b->data.s) + 1);
		if (!buf)
			return NULL;

		sprintf (buf, "%s%s", a->data.s, b->data.s);
		return vm_value (vm, VAL_STR, ((union val_data) { .s = buf }));
	}

	return NULL;
}

Value *vm_list_copy (VM *vm, Value *lst)
{
	if (lst == &nil)
		return &nil;

	return vm_value (vm,
	                 VAL_PAIR,
	                 ((union val_data) { .pair = { lst->data.pair.a, vm_list_copy (vm, lst->data.pair.b) }}));
}

// list concatenation
Value *vm_lcat (VM *vm, Value *a, Value *b)
{
	Value *it;

	if (b != &nil && b->type != VAL_PAIR)
		b = vm_value (vm, VAL_PAIR, ((union val_data) { .pair = { b, &nil }}));

	if (a->type == VAL_PAIR) {
		a = it = vm_list_copy (vm, a);
		while (it->data.pair.b != &nil) // go to the end of this list
			it = it->data.pair.b;
		it->data.pair.b = b;

		return a;
	}
	else
		return vm_value (vm, VAL_PAIR, ((union val_data) { .pair = { a, b }}));
}
		
void vm_dispatch (VM *vm, Chunk *ch, Closure *cl)
{
	Value *v1, *v2, *v3;
	Scope sc = { .pc = 0, .ch = ch, .next = vm->sc };

	array_init (sc.vars);
	vm->sc = &sc;

	while (1) {
	//	printf ("op: %x\n", ch->code[sc.pc]);
		switch (ch->code[sc.pc++]) {
			case OP_NOP: break;
			case OP_ADD: numop (+);
			case OP_SUB: numop (-);
			case OP_MUL: numop (*);
			case OP_DIV: numop (/);
			case OP_MOD: // can't use numop here because we can't use doubles
				v2 = pop ();
				v1 = pop ();
				if (v1->type == VAL_INT && v2->type == VAL_INT)
					vm_push (vm,
					         vm_value (vm, VAL_INT, ((union val_data) { .i = v1->data.i % v2->data.i })));
				break;
			case OP_EQ: numop (==);
			case OP_LT: numop (<);
			case OP_LTE: numop (<=);
			case OP_GT: numop (>);
			case OP_GTE: numop (>=);
			case OP_ANDL: // eventually use a boolean type for these, maybe
				v2 = pop ();
				v1 = pop ();
				vm_push (vm,
				         vm_value (vm, VAL_INT, ((union val_data) { .i = vm_true (v1) && vm_true (v2) })));
				break;
			case OP_ORL:
				v2 = pop ();
				v1 = pop ();
				vm_push (vm,
				         vm_value (vm, VAL_INT, ((union val_data) { .i = vm_true (v1) && vm_true (v2) })));
				break;
			case OP_CAT:
				v2 = pop ();
				v1 = pop ();
				vm_push (vm, vm_cat (vm, v1, v2));
				break;
			case OP_LCAT:
				v2 = pop ();
				v1 = pop ();
				vm_push (vm, vm_lcat (vm, v1, v2));
				break;
			case OP_PSHI:
				vm_push (vm, vm_value (vm, VAL_INT, ((union val_data) { .i = vm_read32 (vm) })));
				break;
			case OP_PSHD:
				vm_push (vm, vm_value (vm, VAL_DBL, ((union val_data) { .d = vm_readdouble (vm) })));
				break;
			case OP_PSHS:
				vm_push (vm, vm_value (vm, VAL_STR, ((union val_data) { .s = vm_readstring (vm) })));
				break;
			case OP_PSHV:
				vm_push (vm, sc.vars[vm_read32 (vm) - 1]);
				break;
			case OP_UVAL:
				if (cl)
					vm_push (vm, cl->upvals[vm_read32 (vm) - 1]);
				break;
			case OP_SET:
				array_add_at (sc.vars, vm->stack[vm->sp - 1], vm_read32 (vm) - 1);
				break;
			case OP_POP:
				array_add_at (sc.vars, pop (), vm_read32 (vm) - 1);
				break;
			case OP_CLSR:
				vm_push (vm, vm_value (vm, VAL_CLSR, ((union val_data) { .cl = vm_closure (vm) })));
				break;
			case OP_SELF:
				vm_push (vm, vm_value (vm, VAL_CLSR, ((union val_data) { .cl = cl })));
				break;
			case OP_NIL:
				vm_push (vm, &nil);
				break;
			case OP_JMP:
				vm->sc->pc = vm_read32 (vm);
				break;
			case OP_JNZ:
				v1 = pop ();
				if (vm_true (v1))
					vm->sc->pc = vm_read32 (vm);
				else
					vm->sc->pc += 4;
				break;
			case OP_JZ:
				v1 = pop ();
				if (vm_false (v1))
					vm->sc->pc = vm_read32 (vm);
				else
					vm->sc->pc += 4;
				break;
			case OP_CALL:
				v1 = pop ();
				if (v1->type == VAL_CLSR) {
					//printf ("calling 0x%lx\n", (uint64_t)v1->data.cl);
					vm_dispatch (vm, v1->data.cl->ch, v1->data.cl);
				}
				break;
			case OP_HEAD:
				v1 = pop ();
				if (v1->type == VAL_PAIR)
					vm_push (vm, v1->data.pair.a);
				break;
			case OP_TAIL:
				v1 = pop ();
				if (v1->type == VAL_PAIR)
					vm_push (vm, v1->data.pair.b);
				break;
			case OP_PRNT:
				v1 = pop ();
				vm_print (v1);
				printf ("\n");
				break;
			case OP_RET:
			case OP_END:
				free (vm->sc->vars);
				vm->sc = vm->sc->next;
			//	gc_collect (vm->gc);
				return;
			default: break;
		}
	}
}

VM *vm_init (uint32_t init_ss)
{
	VM *ret = malloc (sizeof (*ret));

	if (!ret)
		return NULL;

	ret->stack = malloc (init_ss * sizeof (Value*));

	if (!ret->stack) {
		free (ret);
		return NULL;
	}

	ret->sp = ret->st = 0;
	ret->ss = init_ss;

	ret->sc = NULL;

	ret->gc = gc_init (ret, 2 * 1024);

	return ret;
}
