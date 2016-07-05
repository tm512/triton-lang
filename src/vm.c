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

struct tn_value nil = { VAL_NIL, ((union tn_val_data) { 0 }) };

static struct tn_value *tn_vm_value (struct tn_vm *vm, enum tn_val_type type, union tn_val_data data)
{
	struct tn_value *ret = tn_gc_alloc (vm->gc);

	if (!ret)
		return NULL;

	ret->type = type;
	ret->data = data;

	return ret;
}

static uint16_t tn_vm_read16 (struct tn_vm *vm)
{
	uint16_t i;

	i = vm->sc->ch->code[vm->sc->pc++];
	i |= vm->sc->ch->code[vm->sc->pc++] << 8;

	return i;
}

static uint32_t tn_vm_read32 (struct tn_vm *vm)
{
	uint32_t i;

	i = vm->sc->ch->code[vm->sc->pc++];
	i |= vm->sc->ch->code[vm->sc->pc++] << 8;
	i |= vm->sc->ch->code[vm->sc->pc++] << 16;
	i |= vm->sc->ch->code[vm->sc->pc++] << 24;

	return i;
}

static uint64_t tn_vm_read64 (struct tn_vm *vm)
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

static double tn_vm_readdouble (struct tn_vm *vm)
{
	uint64_t u64 = tn_vm_read64 (vm);
	double *d = (double*)&u64;
	return *d;
}

static char *tn_vm_readstring (struct tn_vm *vm)
{
	uint16_t len;
	char *ret;

	len = tn_vm_read16 (vm);
	ret = strndup ((char*)vm->sc->ch->code + vm->sc->pc, len);
	if (!ret)
		return NULL;

	vm->sc->pc += len;
	return ret;
}

static inline void tn_vm_push (struct tn_vm *vm, struct tn_value *val)
{
	if (vm->sp == vm->ss) {
		vm->ss *= 2;
		vm->stack = realloc (vm->stack, vm->ss * sizeof (struct tn_value*));

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

void tn_vm_print (struct tn_value *val)
{
	struct tn_value *it;
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
				tn_vm_print (it->data.pair.a);
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
		tn_vm_push (vm, tn_vm_value (vm, VAL_INT, ((union tn_val_data) { .i = v1->data.i OP v2->data.i }))); \
	else if (v1d && v2d) \
		tn_vm_push (vm, tn_vm_value (vm, VAL_DBL, ((union tn_val_data) { .d = v1->data.d OP v2->data.d }))); \
	else if (v1i && v2d) \
		tn_vm_push (vm, tn_vm_value (vm, VAL_DBL, ((union tn_val_data) { .d = v1->data.i OP v2->data.d }))); \
	else if (v1d && v2i) \
		tn_vm_push (vm, tn_vm_value (vm, VAL_DBL, ((union tn_val_data) { .d = v1->data.d OP v2->data.i }))); \
} \
break

static struct tn_scope *tn_vm_scope_copy (struct tn_scope *sc)
{
	struct tn_scope *ret;

	if (!sc)
		return NULL;

	ret = malloc (sizeof (*ret));

	if (!ret) {
		error ("malloc failed\n");
		return NULL;
	}

	ret->pc = sc->pc;
	ret->keep = 0;
	ret->ch = sc->ch;
	ret->vars = sc->vars;
	ret->vars->refs++;
	ret->next = tn_vm_scope_copy (sc->next);

	return ret;
}

static struct tn_closure *tn_vm_closure (struct tn_vm *vm)
{
	struct tn_closure *ret = malloc (sizeof (*ret));
	struct tn_scope *it;
	uint16_t i, nargs;

	if (!ret)
		return NULL;

	ret->ch = vm->sc->ch->subch[tn_vm_read16 (vm)];
	ret->sc = tn_vm_scope_copy (vm->sc);
//	array_init (ret->upvals);
//	nargs = tn_vm_read16 (vm);

//	for (i = nargs; i > 0; i--)
//		array_add_at (ret->upvals, vm->stack[--vm->sp], i - 1);

	return ret;
}

int tn_vm_true (struct tn_value *v)
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

int tn_vm_false (struct tn_value *v)
{
	return !tn_vm_true (v);
}

struct tn_value *tn_vm_cat (struct tn_vm *vm, struct tn_value *a, struct tn_value *b)
{
	char *buf;

	if (a->type == VAL_STR && b->type == VAL_STR) {
		buf = malloc (strlen (a->data.s) + strlen (b->data.s) + 1);
		if (!buf)
			return NULL;

		sprintf (buf, "%s%s", a->data.s, b->data.s);
		return tn_vm_value (vm, VAL_STR, ((union tn_val_data) { .s = buf }));
	}

	return NULL;
}

struct tn_value *tn_vm_list_copy (struct tn_vm *vm, struct tn_value *lst)
{
	if (lst == &nil)
		return &nil;

	return tn_vm_value (vm,
	                 VAL_PAIR,
	                 ((union tn_val_data) { .pair = { lst->data.pair.a, tn_vm_list_copy (vm, lst->data.pair.b) }}));
}

// list concatenation
struct tn_value *tn_vm_lcat (struct tn_vm *vm, struct tn_value *a, struct tn_value *b)
{
	struct tn_value *it;

	if (b != &nil && b->type != VAL_PAIR)
		b = tn_vm_value (vm, VAL_PAIR, ((union tn_val_data) { .pair = { b, &nil }}));

	if (a->type == VAL_PAIR) {
		a = it = tn_vm_list_copy (vm, a);
		while (it->data.pair.b != &nil) // go to the end of this list
			it = it->data.pair.b;
		it->data.pair.b = b;

		return a;
	}
	else
		return tn_vm_value (vm, VAL_PAIR, ((union tn_val_data) { .pair = { a, b }}));
}

struct tn_scope *tn_vm_scope (uint8_t keep)
{
	struct tn_scope *ret = malloc (sizeof (*ret));
	struct tn_scope_vars *vars = malloc (sizeof (*vars));

	if (!ret || !vars)
		goto error;

	ret->keep = keep;
	ret->vars = vars;
	array_init (ret->vars->arr);
	ret->vars->refs = 1;
	ret->next = NULL;

	if (!ret->vars->arr)
		goto error;

	return ret;

error:
	free (ret);
	free (vars);
	return NULL;
}

void tn_vm_dispatch (struct tn_vm *vm, struct tn_chunk *ch, struct tn_value *cl, struct tn_scope *sc)
{
	struct tn_value *v1, *v2, *v3;

	// set up the current scope
	if (!sc) {
		sc = tn_vm_scope (0);
		if (!sc) {
			error ("couldn't allocate a new scope\n");
			return;
		}
	}

	sc->pc = 0;
	sc->ch = ch;
	//sc->refs++;
	sc->next = cl ? cl->data.cl->sc : vm->sc;
	sc->gc_next = vm->sc; // the GC needs to traverse the real call stack
	vm->sc = sc;

	if (vm->sc == vm->sc->next)
		error ("scope loop\n");

	// execute the chunk's code
	while (1) {
	//	printf ("op: %x\n", ch->code[sc->pc]);
		switch (ch->code[sc->pc++]) {
			case OP_NOP: break;
			case OP_ADD: numop (+);
			case OP_SUB: numop (-);
			case OP_MUL: numop (*);
			case OP_DIV: numop (/);
			case OP_MOD: // can't use numop here because we can't use doubles
				v2 = pop ();
				v1 = pop ();
				if (v1->type == VAL_INT && v2->type == VAL_INT)
					tn_vm_push (vm,
					         tn_vm_value (vm, VAL_INT, ((union tn_val_data) { .i = v1->data.i % v2->data.i })));
				break;
			case OP_EQ: numop (==);
			case OP_LT: numop (<);
			case OP_LTE: numop (<=);
			case OP_GT: numop (>);
			case OP_GTE: numop (>=);
			case OP_ANDL: // eventually use a boolean type for these, maybe
				v2 = pop ();
				v1 = pop ();
				tn_vm_push (vm,
				         tn_vm_value (vm, VAL_INT, ((union tn_val_data) { .i = tn_vm_true (v1) && tn_vm_true (v2) })));
				break;
			case OP_ORL:
				v2 = pop ();
				v1 = pop ();
				tn_vm_push (vm,
				         tn_vm_value (vm, VAL_INT, ((union tn_val_data) { .i = tn_vm_true (v1) && tn_vm_true (v2) })));
				break;
			case OP_CAT:
				v2 = pop ();
				v1 = pop ();
				tn_vm_push (vm, tn_vm_cat (vm, v1, v2));
				break;
			case OP_LCAT:
				v2 = pop ();
				v1 = pop ();
				tn_vm_push (vm, tn_vm_lcat (vm, v1, v2));
				break;
			case OP_PSHI:
				tn_vm_push (vm, tn_vm_value (vm, VAL_INT, ((union tn_val_data) { .i = tn_vm_read32 (vm) })));
				break;
			case OP_PSHD:
				tn_vm_push (vm, tn_vm_value (vm, VAL_DBL, ((union tn_val_data) { .d = tn_vm_readdouble (vm) })));
				break;
			case OP_PSHS:
				tn_vm_push (vm, tn_vm_value (vm, VAL_STR, ((union tn_val_data) { .s = tn_vm_readstring (vm) })));
				break;
			case OP_PSHV: {
				struct tn_scope *s = sc;
				uint16_t depth = tn_vm_read16 (vm);

				while (depth--)
					s = s->next;

				tn_vm_push (vm, s->vars->arr[tn_vm_read32 (vm) - 1]);
				break;
			}
			case OP_SET:
				array_add_at (sc->vars->arr, vm->stack[vm->sp - 1], tn_vm_read32 (vm) - 1);
				break;
			case OP_POP: {
				uint32_t id = tn_vm_read32 (vm);
				if (id > 0)
					array_add_at (sc->vars->arr, pop (), id - 1);
				else
					pop ();
				break;
			}
			case OP_CLSR:
				tn_vm_push (vm, tn_vm_value (vm, VAL_CLSR, ((union tn_val_data) { .cl = tn_vm_closure (vm) })));
				break;
			case OP_SELF:
				if (cl)
					tn_vm_push (vm, cl);
				break;
			case OP_NIL:
				tn_vm_push (vm, &nil);
				break;
			case OP_JMP:
				vm->sc->pc = tn_vm_read32 (vm);
				break;
			case OP_JNZ:
				v1 = pop ();
				if (tn_vm_true (v1))
					vm->sc->pc = tn_vm_read32 (vm);
				else
					vm->sc->pc += 4;
				break;
			case OP_JZ:
				v1 = pop ();
				if (tn_vm_false (v1))
					vm->sc->pc = tn_vm_read32 (vm);
				else
					vm->sc->pc += 4;
				break;
			case OP_CALL:
				v1 = pop ();
				if (v1->type == VAL_CLSR) {
					//printf ("calling 0x%lx\n", (uint64_t)v1->data.cl);
					tn_vm_dispatch (vm, v1->data.cl->ch, v1, NULL);
					vm->sc = sc;
				}
				break;
			case OP_HEAD:
				v1 = pop ();
				if (v1->type == VAL_PAIR)
					tn_vm_push (vm, v1->data.pair.a);
				break;
			case OP_TAIL:
				v1 = pop ();
				if (v1->type == VAL_PAIR)
					tn_vm_push (vm, v1->data.pair.b);
				break;
			case OP_PRNT:
				v1 = pop ();
				tn_vm_print (v1);
				printf ("\n");
				tn_vm_push (vm, &nil);
				break;
			case OP_RET:
			case OP_END:
				if (!vm->sc->keep) {
					if (--vm->sc->vars->refs == 0) {
						free (vm->sc->vars->arr);
						free (vm->sc->vars);
					}
					free (vm->sc);
				}

				vm->sc = NULL;
				return;
			default: break;
		}
	}
}

struct tn_vm *tn_vm_init (uint32_t init_ss)
{
	struct tn_vm *ret = malloc (sizeof (*ret));

	if (!ret)
		return NULL;

	ret->stack = malloc (init_ss * sizeof (struct tn_value*));

	if (!ret->stack) {
		free (ret);
		return NULL;
	}

	ret->sp = ret->st = 0;
	ret->ss = init_ss;

	ret->sc = NULL;

	ret->gc = tn_gc_init (ret, sizeof (struct tn_value) * 10);

	return ret;
}
