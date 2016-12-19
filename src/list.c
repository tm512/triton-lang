#include <stdio.h>

#include "error.h"
#include "hash.h"
#include "value.h"
#include "gc.h"
#include "vm.h"

static void tn_list_map (struct tn_vm *vm, int argn)
{
	int i, anynil = 0;
	uint8_t gc_on = vm->gc->on;
	struct tn_value *fn = vm->stack[vm->sp - 1], *ret = &nil, **tail = &ret;
	struct tn_value **lists = &vm->stack[vm->sp - argn];
	struct tn_scope *sc;

	if (fn->type != VAL_CLSR && fn->type != VAL_CFUN) {
		error ("non-function function argument passed to list:map\n");
		tn_vm_push (vm, &nil);
		return;
	}

	for (i = 0; i < argn - 1; i++) {
		if (lists[i]->type != VAL_PAIR) {
			error ("non-list passed to list:map\n");
			tn_vm_push (vm, &nil);
			return;
		}
	}

	vm->gc->on = 0; // disable the GC for this since it won't find a lot of the values we work on

	while (1) {
		// recursively push one value from each list passed, then increment the head of that list
		for (i = argn - 2; i >= 0; i--) {
			if (lists[i] != &nil) {
				tn_vm_push (vm, lists[i]->data.pair.a);
				lists[i] = lists[i]->data.pair.b;
			}
			else {
				anynil = 1;
				break;
			}
		}

		if (anynil)
			break;

		if (fn->type == VAL_CLSR) {
			sc = vm->sc;
			tn_vm_dispatch (vm, fn->data.cl->ch, fn, NULL, argn - 1);
			vm->sc = sc;
		}
		else if (fn->type == VAL_CFUN)
			fn->data.cfun (vm, argn - 1);

		*tail = tn_pair (vm, tn_vm_pop (vm), &nil);
		tail = &(*tail)->data.pair.b;
	}

	vm->sp -= argn;
	tn_vm_push (vm, ret);
	vm->gc->on = gc_on;
}

static void tn_list_filter (struct tn_vm *vm, int argn)
{
	uint8_t gc_on = vm->gc->on;
	struct tn_value *fn, *lst;
	struct tn_value *ret = &nil, **tail = &ret;
	struct tn_scope *sc;

	if (argn != 2 || tn_value_get_args (vm, "al", &fn, &lst) || (fn->type != VAL_CLSR && fn->type != VAL_CFUN)) {
		error ("invalid arguments passed to list:filter\n");
		tn_vm_push (vm, &nil);
		return;
	}

	vm->gc->on = 0;

	while (lst != &nil) {
		tn_vm_push (vm, lst->data.pair.a);

		if (fn->type == VAL_CLSR) {
			sc = vm->sc;
			tn_vm_dispatch (vm, fn->data.cl->ch, fn, NULL, 1);
			vm->sc = sc;
		}
		else if (fn->type == VAL_CFUN)
			fn->data.cfun (vm, argn - 1);

		if (tn_value_true (tn_vm_pop (vm))) {
			*tail = tn_pair (vm, lst->data.pair.a, &nil);
			tail = &(*tail)->data.pair.b;
		}

		lst = lst->data.pair.b;
	}

	tn_vm_push (vm, ret);
	vm->gc->on = gc_on;
}

static void tn_list_length (struct tn_vm *vm, int argn)
{
	int len = 0;
	struct tn_value *lst;

	if (argn != 1 || tn_value_get_args (vm, "l", &lst)) {
		error ("invalid argument passed to list:length\n");
		tn_vm_push (vm, &nil);
		return;
	}

	while (lst != &nil) {
		len++;
		lst = lst->data.pair.b;
	}

	tn_vm_push (vm, tn_int (vm, len));
}

struct tn_hash *tn_list_module (struct tn_vm *vm)
{
	struct tn_hash *ret = tn_hash_new (8);

	tn_hash_insert (ret, "map", tn_gc_preserve (tn_cfun (vm, tn_list_map)));
	tn_hash_insert (ret, "filter", tn_gc_preserve (tn_cfun (vm, tn_list_filter)));
	tn_hash_insert (ret, "length", tn_gc_preserve (tn_cfun (vm, tn_list_length)));

	return ret;
}
