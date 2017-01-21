#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "error.h"
#include "value.h"
#include "gc.h"
#include "vm.h"

struct tn_value nil = { VAL_NIL, { 0 } };
struct tn_value lststart = { VAL_NIL, { 0 } };

struct tn_value *tn_value_new (struct tn_vm *vm, enum tn_val_type type, union tn_val_data data)
{
	struct tn_value *ret = tn_gc_alloc (vm->gc);

	if (!ret) {
		vm->error = 1;
		return NULL;
	}

	ret->type = type;
	ret->data = data;
	ret->flags = 0;

	return ret;
}

int tn_value_true (struct tn_value *v)
{
	if (!v)
		return 0;

	switch (v->type) {
		case VAL_NIL: return 0;
		case VAL_INT: return !!v->data.i;
		case VAL_PAIR: return 1;
		default: return 0;
	}
}

int tn_value_false (struct tn_value *v)
{
	return !tn_value_true (v);
}

struct tn_value *tn_value_cat (struct tn_vm *vm, struct tn_value *a, struct tn_value *b)
{
	char *buf;

	if (a->type == VAL_STR && b->type == VAL_STR) {
		buf = malloc (strlen (a->data.s) + strlen (b->data.s) + 1);
		if (!buf) {
			error ("malloc failed\n");
			vm->error = 1;
			return NULL;
		}

		sprintf (buf, "%s%s", a->data.s, b->data.s);
		return tn_string (vm, buf);
	}
	else {
		error ("non-string passed to concatenate operator\n");
		vm->error = 1;
		return NULL;
	}

	return NULL;
}

struct tn_value *tn_value_lcopy (struct tn_vm *vm, struct tn_value *lst, struct tn_value **last)
{
	uint8_t gc_on = vm->gc->on;
	struct tn_value *ret = &nil, **tail = &ret;

	vm->gc->on = 0;

	while (lst != &nil) {
		*tail = tn_pair (vm, lst->data.pair.a, &nil);
		*last = *tail;
		tail = &(*tail)->data.pair.b;

		lst = lst->data.pair.b;
	}

	vm->gc->on = gc_on;
	return ret;
}

// list concatenation
struct tn_value *tn_value_lcat (struct tn_vm *vm, struct tn_value *a, struct tn_value *b)
{
	struct tn_value *ret;

	// to make this a proper list, we need the second element to be a pair too
	// this requires a bit of working with the GC
	if (b != &nil && b->type != VAL_PAIR) {
		b = tn_gc_preserve (tn_pair (vm, b, &nil));
		ret = tn_pair (vm, a, b);
		tn_gc_release (b);

		return ret;
	}

	return tn_pair (vm, a, b);
}

struct tn_value *tn_value_lcon (struct tn_vm *vm, int n)
{
	struct tn_value *val;

	if (n == 0)
		return &nil;

	val = tn_gc_preserve (tn_vm_pop (vm));
	val = tn_pair (vm, val, tn_value_lcon (vm, n - 1));
	tn_gc_release (val->data.pair.a);

	return val;
}

struct tn_value *tn_value_lste (struct tn_vm *vm)
{
	struct tn_value *tail, *val = tn_vm_pop (vm);

	if (val == &lststart)
		return &nil;

	tn_gc_preserve (val);
	tail = tn_value_lste (vm);
	val = tn_pair (vm, val, tail);
	tn_gc_preserve (val);

	return val;
}

char *tn_value_string (struct tn_value *val)
{
	char *ret = NULL, *old, *tmp;
	struct tn_value *it;

	switch (val->type) {
		case VAL_NIL:
			asprintf (&ret, "nil");
			break;
		case VAL_INT:
			asprintf (&ret, "%i", val->data.i);
			break;
		case VAL_DBL:
			asprintf (&ret, "%g", val->data.d);
			break;
		case VAL_STR:
			asprintf (&ret, "%s", val->data.s);
			break;
		case VAL_PAIR:
			it = val;

			asprintf (&ret, "[");
			while (it != &nil) {
				old = ret;
				tmp = tn_value_string (it->data.pair.a);

				asprintf (&ret, "%s%s%s", old, tmp, (it->data.pair.b != &nil) ? " " : "");
				free (old);
				free (tmp);

				it = it->data.pair.b;
			}
			old = ret;
			asprintf (&ret, "%s]", old);
			free (old);
			break;
		case VAL_CLSR:
			asprintf (&ret, "closure:0x%lx", (uint64_t)val->data.cl);
			break;
		case VAL_CFUN:
			asprintf (&ret, "cfun:0x%lx", (uint64_t)val->data.cfun);
			break;
		case VAL_CMOD:
			asprintf (&ret, "cmod:0x%lx", (uint64_t)val->data.cmod);
			break;
		case VAL_CVAL:
			asprintf (&ret, "cval:0x%lx", (uint64_t)val->data.cval.v);
			break;
		case VAL_SCOPE:
			asprintf (&ret, "scope:0x%lx", (uint64_t)val->data.sc);
			break;
		case VAL_REF:
			tmp = tn_value_string (*val->data.ref);
			asprintf (&ret, "ref:%s", tmp);
			free (tmp);
			break;
		default: break;
	}

	return ret;
}

int tn_value_get_args (struct tn_vm *vm, const char *types, ...)
{
	struct tn_value *val;
	va_list va;

	va_start (va, types);

	while (*types) {
		val = tn_vm_pop (vm);

		if (!val)
			goto error;

		// now decide what to cast this to
		switch (*types) {
			case 'i': {
				int *i = va_arg (va, int*);

				if (val->type != VAL_INT)
					goto error;

				*i = val->data.i;
				break;
			}
			case 'd': {
				double *d = va_arg (va, double*);

				if (val->type != VAL_DBL)
					goto error;

				*d = val->data.d;
				break;
			}
			case 's': {
				char **s = va_arg (va, char**);

				if (val->type != VAL_STR)
					goto error;

				*s = val->data.s;
				break;
			}
			case 'v': { // C value
				void **v = va_arg (va, void**);

				if (val->type != VAL_CVAL)
					goto error;

				*v = val->data.cval.v;
				break;
			}
			case 'l': // list
				if (val->type != VAL_PAIR)
					goto error;
			case 'c': // closure
				if (*types == 'c' && val->type != VAL_CLSR)
					goto error;
			case 'C': // C function
				if (*types == 'C' && val->type != VAL_CFUN)
					goto error;
			case 'a': { // any value
				struct tn_value **v = va_arg (va, struct tn_value**);
				*v = val;
				break;
			}
		}

		types++;
	}

	va_end (va);
	return 0;

error:
	va_end (va);
	return 1;
}
