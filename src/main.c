#include <stdio.h>

#include "error.h"
#include "lexer.h"
#include "parser.h"
#include "gen.h"
#include "vm.h"

int tn_builtin_init (struct tn_vm *vm);
int main (int argc, char **argv)
{
	int repl = 0;
	char line[4096];
	struct tn_token *tok = NULL, *last = NULL, *bak;
	struct tn_expr *ast = NULL;
	struct tn_chunk *code = NULL;
	struct tn_scope *sc = tn_vm_scope (1); // acts as a global scope for the REPL
	struct tn_vm *vm = tn_vm_init (1024);

	if (!sc) {
		error ("failed to allocate scope\n");
		return 1;
	}

	// we can load some builtins here
	// ugly but temporary:
	{
		tok = bak = tn_lexer_tokenize ("fn map(f,l) l ? f(l:h) :: map(f,l:t) nil ;"
		                               "fn range(s,e) s < e ? s :: range(s+1,e) e ;"
		                               "nil", &last);	
	}

	tn_builtin_init (vm);

	if (argc > 1)
		last->next = tn_lexer_tokenize_file (fopen (argv[1], "r"));
	else {
		printf ("triton " GITVER "\n\n");
		repl = 1;
	}

	while (tok || repl) {
		if (tok) {
			ast = tn_parser_body (&tok);

			if (!ast) {
				tok = NULL;
				tn_lexer_free_tokens (bak);
				error ("parsing failed\n");
				continue;
			}

			code = tn_gen_compile (ast, NULL, NULL, code ? code->vartree : NULL);
			tn_parser_free (ast);

			if (!code) {
				tok = NULL;
				tn_lexer_free_tokens (bak);
				error ("compilation failed\n");
				continue;
			}

			tn_disasm (code);
			tn_vm_dispatch (vm, code, NULL, sc, 0);

			if (vm->error) {
				vm->error = 0;
				tok = NULL;
				tn_lexer_free_tokens (bak);
				continue;
			}

			if (repl) {
				tn_vm_print (vm->stack[--vm->sp]);
				printf ("\n");
			}
		}

		if (repl) {
			printf ("> ");
			fgets (line, 4096, stdin);
			tok = bak = tn_lexer_tokenize (line, NULL);
		}
	}

	return 0;
}
