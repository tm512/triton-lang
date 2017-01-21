#include <stdio.h>

#include "error.h"
#include "value.h"
#include "gc.h"
#include "vm.h"

static int tn_builtin_ldecon (struct tn_vm *vm, struct tn_value *lst)
{
	int ret = 0;

	if (lst == &nil)
		return 0;

	ret = tn_builtin_ldecon (vm, lst->data.pair.b);
	tn_vm_push (vm, lst->data.pair.a);
	ret++;

	return ret;
}

static void tn_builtin_apply (struct tn_vm *vm, int n)
{
	// fn (1, 2, 3) == 3 2 1 fn call
	// apply (fn, [1 2 3]) == [1 2 3] fn apply call
	int nargs;
	struct tn_value *fn, *args;
	struct tn_scope *sc;
	
	if (n != 2 || tn_value_get_args (vm, "al", &fn, &args)) {
		error ("invalid arguments passed to apply\n");
		tn_vm_push (vm, &nil);
		return;
	}

	// arguments are passed in via a list, so we recursively push every element of it
	nargs = tn_builtin_ldecon (vm, args);

	if (fn->type == VAL_CLSR) {
		sc = vm->sc;
		tn_vm_exec (vm, fn->data.cl->ch, fn, NULL, nargs);
		vm->sc = sc;
	}
	else if (fn->type == VAL_CFUN)
		fn->data.cfun (vm, nargs);
	else {
		error ("non-function passed to apply\n");
		tn_vm_push (vm, &nil);
	}
}

static void tn_builtin_range (struct tn_vm *vm, int n)
{
	int i, start, end, step;
	struct tn_value *lst = &nil, *num;

	if (n < 2 || (n == 2 && tn_value_get_args (vm, "ii", &start, &end))
	 || (n == 3 && tn_value_get_args (vm, "iii", &start, &end, &step))) {
		error ("invalid arguments passed to range\n");
		tn_vm_push (vm, &nil);
		return;
	}

	if (n == 2)
		step = start < end ? 1 : -1;

	end -= start % step; // start % step and end % step need to be the same

	for (i = end; start < end ? i >= start : i <= start; i -= step) {
		num = tn_gc_preserve (tn_int (vm, i));

		lst = tn_gc_preserve (tn_pair (vm, num, lst));
	}

	tn_gc_release_list (lst); 
	tn_vm_push (vm, lst);
}

struct tn_hash *tn_list_module (struct tn_vm *vm);
struct tn_hash *tn_string_module (struct tn_vm *vm);
struct tn_hash *tn_io_module (struct tn_vm *vm);
int tn_builtin_init (struct tn_vm *vm)
{
	tn_vm_setglobal (vm, "apply", tn_cfun (vm, tn_builtin_apply));
	tn_vm_setglobal (vm, "range", tn_cfun (vm, tn_builtin_range));
	tn_vm_setglobal (vm, "string", tn_cmod (vm, tn_string_module (vm)));
	tn_vm_setglobal (vm, "io", tn_cmod (vm, tn_io_module (vm)));
	tn_vm_setglobal (vm, "list", tn_cmod (vm, tn_list_module (vm)));

	return 0;
}
