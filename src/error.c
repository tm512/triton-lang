#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// eventually we'll put in some callbacks here and stuff
void tn_error (const char *fmt, ...)
{
	va_list va;

	va_start (va, fmt);
	fprintf (stderr, "error: ");
	vfprintf (stderr, fmt, va);
	va_end (va);
}
