#include <stdio.h>
#include <unistd.h>

#include "error.h"
#include "lexer.h"
#include "parser.h"
#include "gen.h"
#include "vm.h"
#include "import.h"
#include "load.h"

int tn_builtin_init (struct tn_vm *vm);
int main (int argc, char **argv)
{
	int repl = 0;
	char line[4096];
	struct tn_chunk *code = NULL;
	struct tn_scope *sc = tn_vm_scope (1); // acts as a global scope for the REPL
	struct tn_vm *vm = tn_vm_init (1024);

	if (!sc) {
		tn_error ("failed to allocate scope\n");
		return 1;
	}

	tn_builtin_init (vm);
	tn_import_set_path (".:~/.triton:/usr/share/triton");

	if (argc > 1)
		code = tn_load_file (argv[1], NULL);
	else if (!isatty (fileno (stdin)))
		code = tn_load_file ("-", NULL);
	else {
		printf ("triton " GITVER "\n\n");
		repl = 1;
	}

	do {
		if (code) {
		//	tn_disasm (code);
			tn_vm_exec (vm, code, NULL, sc, 0);

			if (vm->error) {
				vm->error = 0;
				vm->sp = 0;
				code = NULL; // TODO: free this
				continue;
			}

			if (repl) {
				tn_vm_print (tn_vm_pop (vm));
				printf ("\n");
			}
		}

		if (repl) {
			printf ("> ");
			fgets (line, 4096, stdin);
			code = tn_load_string (line, code ? code->vars : NULL);
		}
	} while (repl);

	return 0;
}
