#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "error.h"
#include "hash.h"
#include "value.h"
#include "gc.h"
#include "vm.h"

// string module

// string:format (fmt, [va])
void tn_string_format (struct tn_vm *vm, int argn)
{
	int bufsize = 512;
	const char *fmt;
	char *buf = malloc (bufsize), *tail, *copy, *bak;

	if (!buf) {
		error ("malloc failed\n");
		tn_vm_push (vm, &nil);
		return;
	}

	copy = NULL;
	tail = buf;

	if (argn == 0 || tn_value_get_args (vm, "s", &fmt)) {
		error ("invalid arguments passed to string:format\n");
		tn_vm_push (vm, &nil);
		return;
	}

	while (*fmt) {
		// TODO: options parsing
		if (*fmt == '{') {
			fmt++;
			copy = tn_value_string (tn_vm_pop (vm));
		}
		else if (*fmt == '}') // ignore this for now
			fmt++;
		else
			*tail++ = *fmt++;

		bak = copy;
		if (copy)
			while (*copy)
				*tail++ = *copy++;

		free (bak);
		copy = NULL;
	}

	tn_vm_push (vm, tn_string (vm, buf));
}

// string:length (str)
static void tn_string_length (struct tn_vm *vm, int argn)
{
	char *str;

	if (argn != 1 || tn_value_get_args (vm, "s", &str)) {
		error ("invalid arguments passed to string:length\n");
		tn_vm_push (vm, &nil);
		return;
	}

	tn_vm_push (vm, tn_int (vm, strlen (str)));
}

struct tn_hash *tn_string_module (struct tn_vm *vm)
{
	struct tn_hash *ret = tn_hash_new (8);

	tn_hash_insert (ret, "format", tn_gc_preserve (tn_cfun (vm, tn_string_format)));
	tn_hash_insert (ret, "length", tn_gc_preserve (tn_cfun (vm, tn_string_length)));

	return ret;
}
