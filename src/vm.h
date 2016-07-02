#ifndef VM_H__
#define VM_H__

#include <stdint.h>
#include "array.h"

struct Closure;
typedef struct Value {
	enum val_type {
		VAL_NIL, VAL_IDENT, VAL_INT, VAL_DBL, VAL_STR,
		VAL_PAIR, VAL_CLSR
	} type;

	union val_data {
		int i;
		double d;
		char *s;
		struct {
			struct Value *a, *b;
		} pair;
		struct Closure *cl;
	} data;

	struct Value *next; // for GC
	uint8_t marked;
} Value;

struct BSTNode;
typedef struct Chunk {
	uint8_t code[512];
	uint32_t pc;
	array_def (subch, struct Chunk*);

	// compiler specific stuff, the VM doesn't do anything with this
	const char *name;
	uint32_t maxvar;
	struct BSTNode *vartree;
	array_def (upvals, const char*);
} Chunk;

typedef struct Closure {
	Chunk *ch;
	array_def (upvals, struct Value*);
} Closure;

typedef struct Scope {
	uint32_t pc;
	Chunk *ch;
	array_def (vars, Value*);
	struct Scope *next;
} Scope;

struct GC;
typedef struct VM {
	Value **stack;
	unsigned int sp, st, ss;
	Scope *sc;
	struct GC *gc;
} VM;

void vm_dispatch (VM *vm, Chunk *ch, Closure *cl);
VM *vm_init (uint32_t init_ss);

#endif
