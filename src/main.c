#include <stdio.h>

#include "error.h"
#include "lexer.h"
#include "parser.h"
#include "gen.h"
#include "vm.h"

int main (int argc, char **argv)
{
	int repl = 0;
	char line[4096];
	struct tn_token *tok = NULL, *bak = NULL;
	struct tn_expr *ast;
	struct tn_chunk *code;
	struct tn_vm *vm = tn_vm_init (1024);

	if (argc > 1)
		tok = bak = tn_lexer_tokenize_file (fopen (argv[1], "r"));
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

			code = tn_gen_compile (ast, NULL, NULL, 0);
			tn_parser_free (ast);

			if (!code) {
				tok = NULL;
				tn_lexer_free_tokens (bak);
				error ("compilation failed\n");
				continue;
			}

			tn_vm_dispatch (vm, code, NULL);

			if (repl) {
				tn_vm_print (vm->stack[vm->sp - 1]);
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
