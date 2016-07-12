#ifndef VM_H__
#define VM_H__

#include <stdint.h>
#include "array.h"

struct tn_closure;
struct tn_vm;
struct tn_value {
	enum tn_val_type {
		VAL_NIL, VAL_IDENT, VAL_INT, VAL_DBL, VAL_STR,
		VAL_PAIR, VAL_CLSR, VAL_CFUN
	} type;

	union tn_val_data {
		int i;
		double d;
		char *s;
		struct {
			struct tn_value *a, *b;
		} pair;
		struct tn_closure *cl;
		void (*cfun)(struct tn_vm *vm, int nargs);
	} data;

	struct tn_value *next; // for GC
	uint8_t flags;
};

#define VAL(VM, TYPE, DEF) tn_vm_value (VM, TYPE, ((union tn_val_data) { DEF }))
#define tn_int(VM, I) VAL (VM, VAL_INT, .i = I)
#define tn_double(VM, D) VAL (VM, VAL_DBL, .d = D)
#define tn_string(VM, S) VAL (VM, VAL_STR, .s = S)
#define tn_pair(VM, A, B) tn_vm_value (VM, VAL_PAIR, ((union tn_val_data) { .pair = { A, B } }))
#define tn_closure(VM, CL) VAL (VM, VAL_CLSR, .cl = CL)
#define tn_cfun(VM, FN) VAL (VM, VAL_CFUN, .cfun = FN)

struct tn_bst;
struct tn_chunk {
	uint8_t code[512];
	uint32_t pc;
	array_def (subch, struct tn_chunk*);

	// compiler specific stuff, the VM doesn't do anything with this
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
	unsigned int sp, sb, ss;
	struct tn_scope *sc;
	struct tn_bst *globals;
	struct tn_gc *gc;
};

void tn_vm_dispatch (struct tn_vm *vm, struct tn_chunk *ch, struct tn_value *cl, struct tn_scope *sc, int nargs);
struct tn_scope *tn_vm_scope (uint8_t keep);
void tn_vm_scope_inc_ref (struct tn_scope *sc);
void tn_vm_scope_dec_ref (struct tn_scope *sc);
struct tn_vm *tn_vm_init (uint32_t init_ss);

#endif
