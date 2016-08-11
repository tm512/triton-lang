#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "bst.h"
#include "opcode.h"
#include "parser.h"
#include "gen.h"
#include "value.h"
#include "vm.h"
#include "gc.h"

static inline uint8_t tn_vm_read8 (struct tn_vm *vm)
{
	return vm->sc->ch->code[vm->sc->pc++];
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

void tn_vm_print (struct tn_value*);
void tn_vm_push (struct tn_vm *vm, struct tn_value *val)
{
	if (vm->sp == vm->ss) {
		vm->ss *= 2;
		vm->stack = realloc (vm->stack, vm->ss * sizeof (struct tn_value*));

		if (!vm->stack) {
			error ("realloc failure, could not grow stack\n");
			return;
		}
	}

	vm->stack[vm->sp++] = val;

	if (0)
	{
		int i;
		printf ("stack: ");
		for (i = 0; i < vm->sp; i++) {
			tn_vm_print (vm->stack[i]);
			printf (" ");
		}
		printf ("\n");
	}

	/* this is my "hack" to keep the GC behaved
	   if the GC only scans up to the stack pointer, it could miss
	   operands that were popped, but are still in scope */
	vm->stack[vm->sp] = NULL; // GC should terminate here
}

struct tn_value *tn_vm_pop (struct tn_vm *vm)
{
	if (vm->sp == 0) {
		error ("pop called on empty stack\n");
		return NULL;
	}

	if (vm->stack[--vm->sp]->type == VAL_REF)
		return *vm->stack[vm->sp]->data.ref;

	return vm->stack[vm->sp];
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
		case VAL_CFUN:
			printf ("cfun 0x%lx", (uint64_t)val->data.cfun);
			break;
		case VAL_SCOPE:
			printf ("scope 0x%lx", (uint64_t)val->data.sc);
			break;
		default: break;
	}
}

#define numop(OP) { \
	int v1i, v2i, v1d, v2d; \
	v2 = tn_vm_pop (vm); \
	v1 = tn_vm_pop (vm); \
	v1i = (v1->type == VAL_INT); \
	v2i = (v2->type == VAL_INT); \
	v1d = (v1->type == VAL_DBL); \
	v2d = (v2->type == VAL_DBL); \
	if (v1i && v2i) \
		tn_vm_push (vm, tn_int (vm, v1->data.i OP v2->data.i)); \
	else if (v1d && v2d) \
		tn_vm_push (vm, tn_double (vm, v1->data.d OP v2->data.d)); \
	else if (v1i && v2d) \
		tn_vm_push (vm, tn_double (vm, v1->data.i OP v2->data.d)); \
	else if (v1d && v2i) \
		tn_vm_push (vm, tn_double (vm, v1->data.d OP v2->data.i)); \
	else { \
		error ("non-number passed to numeric operation\n"); \
		vm->error = 1; \
	} \
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
	ret->ch = sc->ch;
	ret->keep = sc->keep;
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

	if (!ret) {
		error ("malloc failed\n");
		vm->error = 1;
		return NULL;
	}

	ret->ch = vm->sc->ch->subch[tn_vm_read16 (vm)];
	ret->sc = tn_vm_scope_copy (vm->sc);

	if (!ret->sc) {
		vm->error = 1;
		return NULL;
	}

	return ret;
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

void tn_vm_free_scope (struct tn_scope *sc)
{
	if (!sc->keep) {
		if (--sc->vars->refs == 0) {
			free (sc->vars->arr);
			free (sc->vars);
		}
		free (sc);
	}
}

void tn_vm_dispatch (struct tn_vm *vm, struct tn_chunk *ch, struct tn_value *cl, struct tn_scope *sc, int nargs)
{
	int tailcall = 0;
	struct tn_value *v1, *v2, *v3;

	// set up the current scope
	if (!sc) {
		sc = tn_vm_scope (0);
		if (!sc) {
			error ("couldn't allocate a new scope\n");
			vm->error = 1;
			return;
		}
	}

	sc->pc = 0;
	sc->ch = ch;
	sc->next = cl ? cl->data.cl->sc : vm->sc;
	sc->gc_next = vm->sc; // the GC needs to traverse the real call stack
	vm->sc = sc;

	// execute the chunk's code
	while (!vm->error) {
		//printf ("op: %x\n", ch->code[sc->pc]);
		switch (ch->code[sc->pc++]) {
			case OP_NOP: break;
			case OP_ADD: numop (+);
			case OP_SUB: numop (-);
			case OP_MUL: numop (*);
			case OP_DIV: numop (/);
			case OP_MOD: // can't use numop here because we can't use doubles
				v2 = tn_vm_pop (vm);
				v1 = tn_vm_pop (vm);
				if (v1->type == VAL_INT && v2->type == VAL_INT)
					tn_vm_push (vm, tn_int (vm, v1->data.i % v2->data.i));
				else {
					error ("non-int passed to modulo\n");
					vm->error = 1;
				}
				break;
			case OP_EQ: numop (==);
			case OP_NEQ: numop (!=);
			case OP_LT: numop (<);
			case OP_LTE: numop (<=);
			case OP_GT: numop (>);
			case OP_GTE: numop (>=);
			case OP_ANDL: // eventually use a boolean type for these, maybe
				v2 = tn_vm_pop (vm);
				v1 = tn_vm_pop (vm);
				tn_vm_push (vm, tn_int (vm, tn_value_true (v1) && tn_value_true (v2)));
				break;
			case OP_ORL:
				v2 = tn_vm_pop (vm);
				v1 = tn_vm_pop (vm);
				tn_vm_push (vm, tn_int (vm, tn_value_true (v1) || tn_value_true (v2)));
				break;
			case OP_CAT:
				v2 = tn_vm_pop (vm);
				v1 = tn_vm_pop (vm);
				tn_vm_push (vm, tn_value_cat (vm, v1, v2));
				break;
			case OP_LCAT:
				v2 = tn_vm_pop (vm);
				v1 = tn_vm_pop (vm);
				tn_vm_push (vm, tn_value_lcat (vm, v1, v2));
				break;
			case OP_PSHI:
				tn_vm_push (vm, tn_int (vm, tn_vm_read32 (vm)));
				break;
			case OP_PSHD:
				tn_vm_push (vm, tn_double (vm, tn_vm_readdouble (vm)));
				break;
			case OP_PSHS:
				tn_vm_push (vm, tn_string (vm, tn_vm_readstring (vm)));
				break;
			case OP_PSHV: {
				struct tn_scope *s = sc;
				uint16_t depth = tn_vm_read16 (vm);
				int32_t i = tn_vm_read32 (vm) - 1;

				while (depth--)
					s = s->next;

				if (i < s->vars->arr_num)
					tn_vm_push (vm, s->vars->arr[i]);
				else {
					error ("unbound variable %i\n", i + 1);
					vm->error = 1;
				}
				break;
			}
			case OP_SET:
				array_add_at (sc->vars->arr, vm->stack[vm->sp - 1], tn_vm_read32 (vm) - 1);
				break;
			case OP_DROP:
				tn_vm_pop (vm);
				break;
			case OP_CLSR:
				tn_vm_push (vm, tn_closure (vm, tn_vm_closure (vm)));
				break;
			case OP_SELF:
				if (cl)
					tn_vm_push (vm, cl);
				break;
			case OP_NIL:
				tn_vm_push (vm, &nil);
				break;
			case OP_GLOB: {
				// with globals, we push a reference to a value, tn_vm_pop will deref it
				char *name = tn_vm_readstring (vm);
				struct tn_value **ref = tn_bst_find_ref (vm->globals, name);

				if (ref)
					tn_vm_push (vm, tn_vref (vm, ref));
				else {
					error ("unbound variable %s\n", name);
					vm->error = 1;
				}
				break;
			}
			case OP_ARGS: {
				int i, op_nargs;
				uint8_t varargs = tn_vm_read8 (vm);

				op_nargs = tn_vm_read32 (vm);

				for (i = 1; i <= op_nargs; i++) {
					if (varargs && i == op_nargs) {
						if (op_nargs <= nargs)
							tn_vm_push (vm, tn_value_lcon (vm, nargs - op_nargs + 1));
						else
							tn_vm_push (vm, &nil);
					}

					array_add_at (sc->vars->arr, tn_vm_pop (vm), i - 1);
				}
				break;
			}
			case OP_JMP:
				vm->sc->pc = tn_vm_read32 (vm);
				break;
			case OP_JNZ:
				v1 = tn_vm_pop (vm);
				if (tn_value_true (v1))
					vm->sc->pc = tn_vm_read32 (vm);
				else
					vm->sc->pc += 4;
				break;
			case OP_JZ:
				v1 = tn_vm_pop (vm);
				if (tn_value_false (v1))
					vm->sc->pc = tn_vm_read32 (vm);
				else
					vm->sc->pc += 4;
				break;
			case OP_TCAL:
				tailcall = 1;
			case OP_CALL:
				v1 = tn_vm_pop (vm);


				if (v1->type == VAL_CLSR) {
					if (tailcall && v1 == cl) {
						sc->pc = 0;
						tailcall = 0;
						continue;
					}

					tn_vm_dispatch (vm, v1->data.cl->ch, v1, NULL, tn_vm_read32 (vm));
					vm->sc = sc;
				}
				else if (v1->type == VAL_CFUN)
					v1->data.cfun (vm, tn_vm_read32 (vm));
				break;
			case OP_ACCS: {
				const char *item = tn_vm_readstring (vm);

				v1 = tn_vm_pop (vm);

				if (v1->type == VAL_PAIR) {
					if (item[0] == 'h' && item[1] == '\0')
						tn_vm_push (vm, v1->data.pair.a);
					else if (item[0] == 't' && item[1] == '\0')
						tn_vm_push (vm, v1->data.pair.b);
					else {
						error ("invalid access to list\n");
						vm->error = 1;
					}
				}

				break;
			}
			case OP_LSTS:
				tn_vm_push (vm, &lststart);
				break;
			case OP_LSTE:
				v1 = tn_value_lste (vm);
				tn_vm_push (vm, v1);
				tn_gc_release_list (v1);
				break;
			case OP_MACC: {
				uint32_t i = tn_vm_read32 (vm) - 1;

				v1 = tn_vm_pop (vm);

				if (v1->type == VAL_SCOPE && i < v1->data.sc->vars->arr_num)
					tn_vm_push (vm, v1->data.sc->vars->arr[i]);

				break;
			}
			case OP_NEG:
				v1 = tn_vm_pop (vm);
				if (v1->type == VAL_INT)
					tn_vm_push (vm, tn_int (vm, -v1->data.i));
				else if (v1->type == VAL_DBL)
					tn_vm_push (vm, tn_double (vm, -v1->data.d));
				break;
			case OP_NOT:
				v1 = tn_vm_pop (vm);
				tn_vm_push (vm, tn_int (vm, tn_value_false (v1)));
				break;
			case OP_IMPT: {
				struct tn_chunk *mod = vm->sc->ch->subch[tn_vm_read16 (vm)];
				struct tn_scope *s = tn_vm_scope (1);

				tn_vm_dispatch (vm, mod, NULL, s, 0);
				tn_vm_push (vm, tn_scope (vm, s));
				vm->sc = sc;
				break;
			}
			case OP_PRNT:
				v1 = tn_vm_pop (vm);
				tn_vm_print (v1);
				printf ("\n");
				tn_vm_push (vm, &nil);
				break;
			case OP_RET:
				goto out;
			default: break;
		}
	}
out:
	tn_vm_free_scope (vm->sc);
	vm->sc = NULL;
	return;
}

void tn_vm_setglobal (struct tn_vm *vm, const char *name, struct tn_value *val)
{
	vm->globals = tn_bst_insert (vm->globals, name, val);

	if (!vm->globals) {
		error ("failed to set global\n");
		vm->error = 1;
	}
}

void tn_apply (struct tn_vm *vm, int n);
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

	ret->sp = ret->sb = 0;
	ret->ss = init_ss;
	ret->error = 0;

	ret->sc = NULL;

	ret->gc = tn_gc_init (ret, sizeof (struct tn_value) * 10);

	if (!ret->gc) {
		free (ret);
		return NULL;
	}

	return ret;
}
