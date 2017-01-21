#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "error.h"
#include "hash.h"
#include "value.h"
#include "gc.h"
#include "vm.h"

static void tn_io_file_free (void *f)
{
	fclose ((FILE*)f);
}

void tn_string_format (struct tn_vm*, int);
static void tn_io_fprintf (struct tn_vm *vm, int argn)
{
	FILE *f;
	const char *fmt;

	if (argn < 2 || tn_value_get_args (vm, "v", &f))
		goto error;

	// call string:format
	tn_string_format (vm, argn - 1);

	// get return value
	if (tn_value_get_args (vm, "s", &fmt))
		goto error;

	tn_vm_push (vm, tn_int (vm, fprintf (f, "%s", fmt)));
	return;

error:
	error ("invalid arguments passed to io:fprintf\n");
	tn_vm_push (vm, &nil);
}

static void tn_io_printf (struct tn_vm *vm, int argn)
{
	// wrapper for fprintf
	tn_vm_push (vm, tn_cval (vm, stdout, NULL));
	tn_io_fprintf (vm, argn + 1);
}

static void tn_io_fopen (struct tn_vm *vm, int argn)
{
	FILE *f;
	const char *path, *mode;

	if (argn != 2 || tn_value_get_args (vm, "ss", &path, &mode)) {
		error ("invalid arguments passed to io:fopen\n");
		tn_vm_push (vm, &nil);
		return;
	}

	f = fopen (path, mode);

	tn_vm_push (vm, f ? tn_cval (vm, f, tn_io_file_free) : &nil);
}

static void tn_io_fclose (struct tn_vm *vm, int argn)
{
	FILE *f;

	if (argn != 1 || tn_value_get_args (vm, "v", &f)) {
		error ("invalid arguments passed to io:fclose\n");
		tn_vm_push (vm, &nil);
		return;
	}

	fclose (f);
	tn_vm_push (vm, &nil);
}

struct tn_hash *tn_io_module (struct tn_vm *vm)
{
	struct tn_hash *ret = tn_hash_new (8);

	tn_hash_insert (ret, "stdin", tn_gc_preserve (tn_cval (vm, stdin, NULL)));
	tn_hash_insert (ret, "stdout", tn_gc_preserve (tn_cval (vm, stdout, NULL)));
	tn_hash_insert (ret, "stderr", tn_gc_preserve (tn_cval (vm, stderr, NULL)));
	tn_hash_insert (ret, "fprintf", tn_gc_preserve (tn_cfun (vm, tn_io_fprintf)));
	tn_hash_insert (ret, "printf", tn_gc_preserve (tn_cfun (vm, tn_io_printf)));
	tn_hash_insert (ret, "fopen", tn_gc_preserve (tn_cfun (vm, tn_io_fopen)));
	tn_hash_insert (ret, "fclose", tn_gc_preserve (tn_cfun (vm, tn_io_fclose)));

	return ret;
}
