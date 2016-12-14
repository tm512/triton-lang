#ifndef VM_H__
#define VM_H__

#include <stdint.h>
#include "array.h"

struct tn_hash;
struct tn_chunk {
	uint8_t *code;
	uint32_t pc, codelen;
	array_def (subch, struct tn_chunk*);

	const char *path;

	// compiler specific stuff, the VM doesn't do anything with this
	const char *name;
	struct tn_chunk_vars {
		uint32_t maxid;
		struct tn_hash *hash;
	} *vars;
	struct tn_chunk *next;
};

struct tn_value;
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
};

struct tn_gc;
struct tn_vm {
	struct tn_value **stack;
	unsigned int sp, sb, ss, error;
	struct tn_scope *sc;
	struct tn_hash *globals;
	struct tn_gc *gc;
};

void tn_vm_push (struct tn_vm *vm, struct tn_value *val);
struct tn_value *tn_vm_pop (struct tn_vm *vm);
void tn_vm_print (struct tn_value *val);
void tn_vm_dispatch (struct tn_vm *vm, struct tn_chunk *ch, struct tn_value *cl, struct tn_scope *sc, int nargs);
struct tn_scope *tn_vm_scope (uint8_t keep);
void tn_vm_scope_inc_ref (struct tn_scope *sc);
void tn_vm_scope_dec_ref (struct tn_scope *sc);
void tn_vm_setglobal (struct tn_vm *vm, const char *name, struct tn_value *val);
struct tn_vm *tn_vm_init (uint32_t init_ss);

#endif
