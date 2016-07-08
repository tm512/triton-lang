#ifndef GC_H__
#define GC_H__

#define GC_MARKED	1
#define GC_NEW		2
#define GC_PRESERVE	4

struct tn_gc {
	struct tn_value *used, *free;
	struct tn_vm *vm;
	uint32_t num_nodes;
};

struct tn_gc *tn_gc_init (struct tn_vm *vm, uint32_t bytes);
struct tn_value *tn_gc_alloc (struct tn_gc *gc);

#endif
