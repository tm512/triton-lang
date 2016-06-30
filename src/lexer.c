#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "parser.h"
#include "lexer.h"
#include "gen.h"
#include "vm.h"

// these need to be in order of descending length, so that we find the longest match first
static Builtin builtins[] = {
	{ "print", TOK_PRNT },
	{ "nil", TOK_NIL },
	{ "do", TOK_DO },
	{ "fn", TOK_FN },
	{ "==", TOK_EQ },
	{ "!=", TOK_NEQ },
	{ "<=", TOK_LTE },
	{ ">=", TOK_GTE },
	{ "&&", TOK_ANDL },
	{ "||", TOK_ORL },
	{ "..", TOK_CAT },
	{ "::", TOK_LCAT },
	{ "=", TOK_ASSN },
	{ "<", TOK_LT },
	{ ">", TOK_GT },
	{ "(", TOK_LPAR },
	{ ")", TOK_RPAR },
	{ ",", TOK_COMM },
	{ "+", TOK_ADD },
	{ "-", TOK_SUB },
	{ "*", TOK_MUL },
	{ "/", TOK_DIV },
	{ "%", TOK_MOD },
	{ ":", TOK_COL },
	{ ";", TOK_SCOL },
	{ "?", TOK_QMRK },
	{ NULL, TOK_ZERO }
};

int lexer_identifier (char **src, Token *ret)
{
	int len;
	char *c = *src;

	// count how many characters we need to copy over
	while (*c && (isalpha (*c) || isdigit (*c) || *c == '_') && ++c);

	len = c - *src;
	ret->type = TOK_IDENT;

	// eventually, we should look up identifiers in a hash table, so that we don't have a bunch of duplicate data
	ret->data.s = strndup (*src, len);
	if (!ret->data.s)
		return 1;

	*src = c;
	return 0;
}

int lexer_number (char **src, Token *ret)
{
	int i = 0, frac = 0;
	double d = 0.0, div = 1;
	char *c = *src;

	// first, we read into the int
	while (isdigit (*c)) {
		i = i * 10 + (*c - '0');
		c++;
	}

	// see if we stopped because of a decimal point
	if (*c == '.') {
		c++;

		// start reading in the fractional portion
		while (isdigit (*c)) {
			frac = frac * 10 + (*c - '0');
			c++;
			div *= 10.0;
		}

		d = i + (frac / div);
		ret->type = TOK_FLOAT;
		ret->data.d = d;
	}
	else if (isalpha (*c) || *c == '_') { // this is an identifier, not a number
		return lexer_identifier (src, ret);
	}
	else {
		ret->type = TOK_INT;
		ret->data.i = i;
	}

	*src = c;
	return 0;
}

int lexer_string (char **src, Token *ret)
{
	char *str;
	char *c = (*src) + 1;
	int len = 0;

	// count string length
	while (*c && *c != '"') {
		len++;
		c += (*c == '\\') ? 2 : 1;
	}

	if (*c == '\0')
		return 1;

	str = malloc (len + 1);
	if (!str)
		return 1;

	*src = c + 1;
	c--; // go back to the very ending of the string
	str[len--] = '\0';

	// go over the string again, copying it over and converting escaped chars
	// this is admittedly pretty ugly
	while (*c != '"') {
		switch (*(c - 1)) {
			case '\\':
				switch (*c) {
					case 'n':
						str[len--] = '\n';
						break;
					default: break;
				}
				c -= 2;
				break;
			default:
				str[len--] = *(c--);
				break;
		}
	}

	ret->type = TOK_STRING;
	ret->data.s = str;
	return 0;
}

Token *lexer_token (char **src)
{
	char *c;
	int i, len;
	Token *ret;

	// skip to the first non-space character
	while (**src && isspace (**src) && ++(*src));
	c = *src;

	// end of string
	if (*c == '\0')
		return NULL;

	ret = malloc (sizeof (*ret));
	if (!ret)
		return NULL;

	// see if the token is a number
	if (isdigit (*c) && !lexer_number (src, ret))
		return ret;

	// see if the token is a string literal
	if (*c == '"' && !lexer_string (src, ret))
		return ret;

	// see if the token is a builtin
	for (i = 0; builtins[i].str; i++) {
		len = strlen (builtins[i].str);
		if (!strncmp (c, builtins[i].str, len)) {
			ret->type = builtins[i].type;
			ret->data.s = builtins[i].str;
			*src = c + len;
			return ret;
		}
	}

	// the last option is that this is an identifier
	if ((isalpha (*c) || *c == '_') && !lexer_identifier (src, ret))
		return ret;

	fprintf (stderr, "error: unrecognized token at: %s\n", c);

	free (ret);
	return NULL;
}

Token *lexer_tokenize (char *src, Token **last)
{
	Token *ret, *it;

	// grab the first token
	ret = it = lexer_token (&src);

	// keep grabbing tokens until we run out
	while (it) {
		if (last)
			*last = it;

		it->next = lexer_token (&src);
		it = it->next;
	}

	return ret;
}

Token *lexer_tokenize_file (FILE *f)
{
	char line[4096];
	Token *ret, *last, *tmp;

	ret = last = tmp = NULL;

	// tokenize first line of file
	while (!last) {
		fgets (line, 4096, f);
		ret = lexer_tokenize (line, &last);
	}

	// keep tokenizing lines, appending to the last token from the last invokation
	while (fgets (line, 4096, f)) {
		last->next = lexer_tokenize (line, &tmp);
		last = tmp ? tmp : last;
	}

	return ret;
}

int main (int argc, char **argv)
{
	Token *tok;
	Expr *ast;
	Chunk *code;
	VM vm = { 0 };

	tok = lexer_tokenize_file (argv[1] ? fopen (argv[1], "r") : stdin);
	ast = parser_body (&tok);
	code = gen_compile (ast, NULL, NULL, 0);
	vm_dispatch (&vm, code, NULL);
	return 0;
}
