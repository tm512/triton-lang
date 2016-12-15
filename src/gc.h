#ifndef GC_H__
#define GC_H__

#define GC_MARKED	1
#define GC_PRESERVE	2

struct tn_gc {
	struct tn_value *used, *free;
	struct tn_vm *vm;
	uint32_t num_nodes;
};

struct tn_gc *tn_gc_init (struct tn_vm *vm, uint32_t bytes);
struct tn_value *tn_gc_preserve (struct tn_value *val);
void tn_gc_release (struct tn_value *val);
void tn_gc_release_list (struct tn_value *lst);
struct tn_value *tn_gc_alloc (struct tn_gc *gc);

#endif
