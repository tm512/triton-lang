#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "error.h"
#include "parser.h"
#include "lexer.h"
#include "gen.h"
#include "vm.h"

// these need to be in order of descending length, so that we find the longest match first
static struct tn_builtin {
	const char *str;
	enum tn_token_type type;
} builtins[] = {
	{ "print", TOK_PRNT },
	{ "nil", TOK_NIL },
	{ "...", TOK_ELPS },
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
	{ "[", TOK_LBRK },
	{ "]", TOK_RBRK },
	{ ",", TOK_COMM },
	{ "+", TOK_ADD },
	{ "-", TOK_SUB },
	{ "*", TOK_MUL },
	{ "/", TOK_DIV },
	{ "%", TOK_MOD },
	{ ":", TOK_COL },
	{ ";", TOK_SCOL },
	{ "?", TOK_QMRK },
	{ "!", TOK_EXCL },
	{ NULL, TOK_ZERO }
};

static int tn_lexer_identifier (char **src, struct tn_token *ret)
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

static int tn_lexer_number (char **src, struct tn_token *ret)
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
		return tn_lexer_identifier (src, ret);
	}
	else {
		ret->type = TOK_INT;
		ret->data.i = i;
	}

	*src = c;
	return 0;
}

static int tn_lexer_string (char **src, struct tn_token *ret)
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

int tn_lexer_comment (char **src)
{
	int block;
	char *c = *src + 1;

	// block comment?
	block = *(c++) == '-';

	while (*c) {
		if (*c == '#' && !tn_lexer_comment (&c))
			return 0;

		// check for comment termination
		if ((block && *c == '-' && *(c + 1) == '#') || (!block && *c == '\n')) {
			*src = c + (block ? 2 : 1);
			return 1;
		}

		c++;
	}

	return !block; // only non-block comments can be terminated by a null character
}

static struct tn_token *tn_lexer_token (char **src)
{
	char *c;
	int i, len;
	struct tn_token *ret;

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
	if (isdigit (*c) && !tn_lexer_number (src, ret))
		return ret;

	// see if the token is a string literal
	if (*c == '"' && !tn_lexer_string (src, ret))
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

	// see if the token is an identifier
	if ((isalpha (*c) || *c == '_') && !tn_lexer_identifier (src, ret))
		return ret;

	free (ret);

	// last option is that this is a comment
	if (*c == '#' && tn_lexer_comment (src))
		return tn_lexer_token (src); // call this again to get the next token

	error ("unrecognized token at: %s\n", c);
	return NULL;
}

struct tn_token *tn_lexer_tokenize (char *src, struct tn_token **last)
{
	struct tn_token *ret, *it;

	// grab the first token
	ret = it = tn_lexer_token (&src);

	// keep grabbing tokens until we run out
	while (it) {
		if (last)
			*last = it;

		it->next = tn_lexer_token (&src);
		it = it->next;
	}

	return ret;
}

struct tn_token *tn_lexer_tokenize_file (FILE *f)
{
	int len = 1;
	char *src, *bak;
	char line[4096];

	src = malloc (4096);

	if (!src) {
		error ("malloc failed");
		return NULL;
	}

	src[0] = '\0';

	// read the file line by line, 
	while (fgets (line, 4096, f)) {
		len += strlen (line);

		bak = src;
		src = realloc (src, len);

		if (!src) {
			error ("realloc failed\n");
			free (bak);
			return NULL;
		}

		strcat (src, line);
	}

	return tn_lexer_tokenize (src, NULL);
}

void tn_lexer_free_tokens (struct tn_token *tok)
{
	struct tn_token *it, *next = tok;
	while ((it = next)) {
		next = it->next;

		if (it->type == TOK_STRING)
			free ((void*)it->data.s);
		free (it);
	}
}
