#ifndef GC_H__
#define GC_H__

typedef struct GC {
	Value *used, *free;
	VM *vm;
	uint32_t num_nodes;
} GC;

GC *gc_init (VM *vm, uint32_t bytes);
Value *gc_alloc (GC *gc);

#endif
