#include <stdio.h>
#include <string.h>

#include "error.h"
#include "lexer.h"
#include "parser.h"
#include "gen.h"
#include "vm.h"

struct tn_chunk *tn_load_tokens (struct tn_token *tok, struct tn_chunk_vars *vars)
{
	struct tn_expr *ast;
	struct tn_chunk *ret;
	struct tn_token *bak = tok;

	ast = tn_parser_body (&tok);

	if (!ast) {
		tn_lexer_free_tokens (bak);
		error ("parsing failed\n");
		return NULL;
	}

	ret = tn_gen_compile (ast, NULL, NULL, vars ? vars : NULL);

	tn_lexer_free_tokens (bak);
	tn_parser_free (ast);

	if (!ret) {
		error ("compilation failed\n");
		return NULL;
	}

	return ret;
}

struct tn_chunk *tn_load_file (const char *path, struct tn_chunk_vars *vars)
{
	FILE *f;
	struct tn_chunk *ret;
	struct tn_token *tok;

	if (!strcmp (path, "-"))
		f = stdin;
	else
		f = fopen (path, "r");

	if (!f)
		return NULL;

	tok = tn_lexer_tokenize_file (f);

	if (!tok) {
		error ("lexing failed\n");
		return NULL;
	}
	
	ret = tn_load_tokens (tok, vars);
	ret->path = path;

	return ret;
}

struct tn_chunk *tn_load_string (const char *str, struct tn_chunk_vars *vars)
{
	struct tn_token *tok;

	tok = tn_lexer_tokenize (str, NULL);

	if (!tok) {
		error ("lexing failed\n");
		return NULL;
	}

	return tn_load_tokens (tok, vars);
}
