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
} Value;

struct BSTNode;
typedef struct Chunk {
	uint8_t code[512];
	uint32_t pc;
	array_def (vars, Value*);
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
} Scope;

typedef struct VM {
	Value *stack[128];
	unsigned int sp;
} VM;

void vm_dispatch (VM *vm, Chunk *ch, Closure *cl);

#endif
