#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "error.h"
#include "value.h"
#include "gc.h"
#include "vm.h"

struct tn_value nil = { VAL_NIL, ((union tn_val_data) { 0 }) };
struct tn_value lststart = { VAL_NIL, ((union tn_val_data) { 0 }) };

struct tn_value *tn_value_new (struct tn_vm *vm, enum tn_val_type type, union tn_val_data data)
{
	struct tn_value *ret = tn_gc_alloc (vm->gc);

	if (!ret)
		return NULL;

	ret->type = type;
	ret->data = data;

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
		if (!buf)
			return NULL;

		sprintf (buf, "%s%s", a->data.s, b->data.s);
		return tn_string (vm, buf);
	}

	return NULL;
}

struct tn_value *tn_value_list_copy (struct tn_vm *vm, struct tn_value *lst)
{
	if (lst == &nil)
		return &nil;

	return tn_pair (vm, lst->data.pair.a, tn_value_list_copy (vm, lst->data.pair.b));
}

// list concatenation
struct tn_value *tn_value_lcat (struct tn_vm *vm, struct tn_value *a, struct tn_value *b)
{
	if (b != &nil && b->type != VAL_PAIR)
		b = tn_pair (vm, b, &nil);

	return tn_pair (vm, a, b);
}

struct tn_value *tn_value_lcon (struct tn_vm *vm, int n)
{
	struct tn_value *val;

	if (n == 0)
		return &nil;

	val = tn_vm_pop (vm);

	return tn_pair (vm, val, tn_value_lcon (vm, n - 1));
}

int tn_value_get_args (struct tn_vm *vm, const char *types, ...)
{
	struct tn_value *val;
	va_list va;

	va_start (va, types);

	while (*types) {
		val = tn_vm_pop (vm);

		if (!val) {
			va_end (va);
			return 1;
		}

		// now decide what to cast this to
		switch (*types) {
			case 'i': {
				int *i = va_arg (va, int*);
				*i = val->data.i;
				break;
			}
			case 'd': {
				double *d = va_arg (va, double*);
				*d = val->data.d;
				break;
			}
			case 's': {
				char **s = va_arg (va, char**);
				*s = val->data.s;
				break;
			}
			case 'l': // list
			case 'c': // closure
			case 'C': // C function
			case 'v': { // any value
				struct tn_value **v = va_arg (va, struct tn_value**);
				*v = val;
				break;
			}
		}

		types++;
	}

	va_end (va);
	return 0;
}
