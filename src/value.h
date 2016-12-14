#ifndef VALUE_H__
#define VALUE_H__

#include <stdint.h>

struct tn_closure;
struct tn_scope;
struct tn_vm;
struct tn_value {
	enum tn_val_type {
		VAL_NIL, VAL_IDENT, VAL_INT, VAL_DBL, VAL_STR,
		VAL_PAIR, VAL_CLSR, VAL_CFUN, VAL_CMOD, VAL_SCOPE,
		VAL_REF
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
		struct tn_scope *sc;
		struct tn_value **ref;
	} data;

	struct tn_value *next; // for GC
	uint8_t flags;
};

#define VAL(VM, TYPE, DEF) tn_value_new (VM, TYPE, ((union tn_val_data) { DEF }))
#define tn_int(VM, I) VAL (VM, VAL_INT, .i = I)
#define tn_double(VM, D) VAL (VM, VAL_DBL, .d = D)
#define tn_string(VM, S) VAL (VM, VAL_STR, .s = S)
#define tn_pair(VM, A, B) tn_value_new (VM, VAL_PAIR, ((union tn_val_data) { .pair = { A, B } }))
#define tn_closure(VM, CL) VAL (VM, VAL_CLSR, .cl = CL)
#define tn_cfun(VM, FN) VAL (VM, VAL_CFUN, .cfun = FN)
#define tn_scope(VM, SC) VAL (VM, VAL_SCOPE, .sc = SC)
#define tn_vref(VM, R) VAL (VM, VAL_REF, .ref = R)

struct tn_value *tn_value_new (struct tn_vm *vm, enum tn_val_type type, union tn_val_data data);
int tn_value_true (struct tn_value *v);
int tn_value_false (struct tn_value *v);
struct tn_value *tn_value_cat (struct tn_vm *vm, struct tn_value *a, struct tn_value *b);
struct tn_value *tn_value_lcat (struct tn_vm *vm, struct tn_value *a, struct tn_value *b);
struct tn_value *tn_value_lcon (struct tn_vm *vm, int n);
struct tn_value *tn_value_lste (struct tn_vm *vm);
int tn_value_get_args (struct tn_vm *vm, const char *types, ...);

extern struct tn_value nil, lststart;

#endif
