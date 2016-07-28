#include <stdio.h>

#include "error.h"
#include "value.h"
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
	struct tn_value *fn, *args, *tmp;
	struct tn_scope *sc;
	
	if (n != 2 || tn_value_get_args (vm, "cl", &fn, &args)) {
		error ("insufficient arguments to apply\n");
		tn_vm_push (vm, &nil);
		return;
	}

	// arguments are passed in via a list, so we recursively push every element of it
	nargs = tn_builtin_ldecon (vm, args);

	sc = vm->sc;
	tn_vm_dispatch (vm, fn->data.cl->ch, fn, NULL, nargs);
	vm->sc = sc;
}

int tn_builtin_init (struct tn_vm *vm)
{
	tn_vm_setglobal (vm, "apply", tn_cfun (vm, tn_builtin_apply));
	return 0;
}
