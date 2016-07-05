#ifndef VM_H__
#define VM_H__

#include <stdint.h>
#include "array.h"

struct tn_closure;
struct tn_value {
	enum tn_val_type {
		VAL_NIL, VAL_IDENT, VAL_INT, VAL_DBL, VAL_STR,
		VAL_PAIR, VAL_CLSR
	} type;

	union tn_val_data {
		int i;
		double d;
		char *s;
		struct {
			struct tn_value *a, *b;
		} pair;
		struct tn_closure *cl;
	} data;

	struct tn_value *next; // for GC
	uint8_t marked;
};

struct tn_bst;
struct tn_chunk {
	uint8_t code[512];
	uint32_t pc;
	array_def (subch, struct tn_chunk*);

	// compiler specific stuff, the struct tn_vm doesn't do anything with this
	const char *name;
	struct tn_bst *vartree;
	struct tn_chunk *next;
};

struct tn_scope {
	uint32_t pc;
	uint8_t keep;
	struct tn_chunk *ch;

	struct tn_scope_vars {
		array_def (arr, struct tn_value*);
		uint32_t refs;
	} *vars;

	struct tn_scope *next, *gc_next;
};

struct tn_closure {
	struct tn_chunk *ch;
	struct tn_scope *sc; // saves the call stack at closure creation
	array_def (upvals, struct tn_value*);
};

struct tn_gc;
struct tn_vm {
	struct tn_value **stack;
	unsigned int sp, st, ss;
	struct tn_scope *sc;
	struct tn_gc *gc;
};

void tn_vm_dispatch (struct tn_vm *vm, struct tn_chunk *ch, struct tn_value *cl, struct tn_scope *sc);
struct tn_scope *tn_vm_scope (uint8_t keep);
void tn_vm_scope_inc_ref (struct tn_scope *sc);
void tn_vm_scope_dec_ref (struct tn_scope *sc);
struct tn_vm *tn_vm_init (uint32_t init_ss);

#endif
