#include <stdio.h>

#include "error.h"
#include "hash.h"
#include "value.h"
#include "gc.h"
#include "vm.h"

static void tn_list_map (struct tn_vm *vm, int argn)
{
	int i, anynil = 0, pushed = 0;
	uint8_t gc_on = vm->gc->on;
	struct tn_value *fn = vm->stack[vm->sp - 1], *ret = &nil, **tail = &ret;
	struct tn_value **lists = &vm->stack[vm->sp - argn];
	struct tn_scope *sc;

	if (fn->type != VAL_CLSR && fn->type != VAL_CFUN) {
		tn_error ("non-function function argument passed to list:map\n");
		tn_vm_push (vm, &nil);
		return;
	}

	for (i = 0; i < argn - 1; i++) {
		if (lists[i]->type != VAL_PAIR) {
			tn_error ("non-list passed to list:map\n");
			tn_vm_push (vm, &nil);
			return;
		}
	}

	vm->gc->on = 0; // disable the GC for this since it won't find a lot of the values we work on

	while (1) {
		// push one value from each list passed, then increment the head of that list
		for (i = argn - 2; i >= 0; i--) {
			if (lists[i] != &nil) {
				pushed++;
				tn_vm_push (vm, lists[i]->data.pair.a);
				lists[i] = lists[i]->data.pair.b;
			}
			else {
				anynil = 1;
				vm->sp -= pushed;
				break;
			}
		}

		if (anynil)
			break;

		if (fn->type == VAL_CLSR) {
			sc = vm->sc;
			tn_vm_exec (vm, fn->data.cl->ch, fn, NULL, argn - 1);
			vm->sc = sc;
		}
		else if (fn->type == VAL_CFUN)
			fn->data.cfun (vm, argn - 1);

		pushed = 0;

		*tail = tn_pair (vm, tn_vm_pop (vm), &nil);
		tail = &(*tail)->data.pair.b;
	}

	vm->sp -= argn;
	tn_vm_push (vm, ret);
	vm->gc->on = gc_on;
}

static void tn_list_foldl (struct tn_vm *vm, int argn)
{
	struct tn_value *fn, *lst, *init;
	struct tn_scope *sc;

	if (argn != 3 || tn_value_get_args (vm, "aaa", &fn, &lst, &init)
	    || (fn->type != VAL_CLSR && fn->type != VAL_CFUN) || (lst != &nil && lst->type != VAL_PAIR)) {
		tn_error ("invalid arguments passed to list:foldl\n");
		tn_vm_push (vm, &nil);
		return;
	}

	tn_vm_push (vm, init);

	// call fn on each node in the list, along with init
	// the return value will be used as init for the next iteration
	while (lst != &nil) {
		tn_vm_push (vm, lst->data.pair.a);

		if (fn->type == VAL_CLSR) {
			sc = vm->sc;
			tn_vm_exec (vm, fn->data.cl->ch, fn, NULL, argn - 1);
			vm->sc = sc;
		}
		else if (fn->type == VAL_CFUN)
			fn->data.cfun (vm, 2);

		lst = lst->data.pair.b;
	}
}

static void tn_list_foldr (struct tn_vm *vm, int argn)
{
	struct tn_value *fn, *lst, *init;
	struct tn_scope *sc;

	if (argn != 3 || tn_value_get_args (vm, "aaa", &fn, &lst, &init)
	    || (fn->type != VAL_CLSR && fn->type != VAL_CFUN) || (lst != &nil && lst->type != VAL_PAIR)) {
		tn_error ("invalid arguments passed to list:foldr\n");
		tn_vm_push (vm, &nil);
		return;
	}

	tn_vm_push (vm, init);

	if (lst != &nil) {
		// call foldr on the tail of lst
		tn_vm_push (vm, lst->data.pair.b);
		tn_vm_push (vm, fn);
		tn_list_foldr (vm, argn);

		// call fn on the head of lst and the return value of foldr
		tn_vm_push (vm, lst->data.pair.a);

		if (fn->type == VAL_CLSR) {
			sc = vm->sc;
			tn_vm_exec (vm, fn->data.cl->ch, fn, NULL, argn - 1);
			vm->sc = sc;
		}
		else if (fn->type == VAL_CFUN)
			fn->data.cfun (vm, 2);
	}
}

static void tn_list_filter (struct tn_vm *vm, int argn)
{
	uint8_t gc_on = vm->gc->on;
	struct tn_value *fn, *lst;
	struct tn_value *ret = &nil, **tail = &ret;
	struct tn_scope *sc;

	if (argn != 2 || tn_value_get_args (vm, "aa", &fn, &lst)
	    || (fn->type != VAL_CLSR && fn->type != VAL_CFUN) || (lst != &nil && lst->type != VAL_PAIR)) {
		tn_error ("invalid arguments passed to list:filter\n");
		tn_vm_push (vm, &nil);
		return;
	}

	vm->gc->on = 0;

	while (lst != &nil) {
		tn_vm_push (vm, lst->data.pair.a);

		if (fn->type == VAL_CLSR) {
			sc = vm->sc;
			tn_vm_exec (vm, fn->data.cl->ch, fn, NULL, 1);
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
		tn_error ("invalid argument passed to list:length\n");
		tn_vm_push (vm, &nil);
		return;
	}

	while (lst != &nil) {
		len++;
		lst = lst->data.pair.b;
	}

	tn_vm_push (vm, tn_int (vm, len));
}

static void tn_list_ref (struct tn_vm *vm, int argn)
{
	int i, n;
	struct tn_value *lst;

	if (argn != 2 || tn_value_get_args (vm, "li", &lst, &n)) {
		tn_error ("invalid argument passed to list:ref\n");
		tn_vm_push (vm, &nil);
		return;
	}

	for (i = 0; i < n && lst != &nil; i++)
		lst = lst->data.pair.b;

	tn_vm_push (vm, lst != &nil ? lst->data.pair.a : &nil);
}

// list:join ([1, 2], [3, 4]) == [1 2 3 4]
static void tn_list_join (struct tn_vm *vm, int argn)
{
	struct tn_value *ret, *last;
	struct tn_value *a, *b;

	if (argn != 2 || tn_value_get_args (vm, "ll", &a, &b)) {
		tn_error ("non-list passed to list:join\n");
		tn_vm_push (vm, &nil);
		return;
	}

	ret = tn_value_lcopy (vm, a, &last);
	last->data.pair.b = b;

	tn_vm_push (vm, ret);
}

static void tn_list_reverse (struct tn_vm *vm, int argn)
{
	struct tn_value *lst, *tail = &nil;

	if (argn != 1 || tn_value_get_args (vm, "l", &lst)) {
		tn_error ("non-list passed to list:reverse\n");
		tn_vm_push (vm, &nil);
		return;
	}

	while (lst != &nil) {
		tail = tn_gc_preserve (tn_pair (vm, lst->data.pair.a, tail));
		lst = lst->data.pair.b;
	}

	tn_gc_release_list (tail);
	tn_vm_push (vm, tail);
}

struct tn_hash *tn_list_module (struct tn_vm *vm)
{
	struct tn_hash *ret = tn_hash_new (8);

	tn_hash_insert (ret, "map", tn_gc_preserve (tn_cfun (vm, tn_list_map)));
	tn_hash_insert (ret, "foldl", tn_gc_preserve (tn_cfun (vm, tn_list_foldl)));
	tn_hash_insert (ret, "foldr", tn_gc_preserve (tn_cfun (vm, tn_list_foldr)));
	tn_hash_insert (ret, "filter", tn_gc_preserve (tn_cfun (vm, tn_list_filter)));
	tn_hash_insert (ret, "length", tn_gc_preserve (tn_cfun (vm, tn_list_length)));
	tn_hash_insert (ret, "ref", tn_gc_preserve (tn_cfun (vm, tn_list_ref)));
	tn_hash_insert (ret, "join", tn_gc_preserve (tn_cfun (vm, tn_list_join)));
	tn_hash_insert (ret, "reverse", tn_gc_preserve (tn_cfun (vm, tn_list_reverse)));

	return ret;
}
