#include <stdio.h>
#include <stdlib.h>

#include "error.h"
#include "vm.h"
#include "gc.h"

#define printf(...) (0) // shut up
Value *gc_init_pool (uint32_t size)
{
	Value *ret;
	int i;

	ret = malloc (size * sizeof (*ret));

	if (!ret)
		return NULL;

	printf ("gc: allocated %u Values\n", size);

	for (i = 0; i < size - 1; i++)
		ret[i].next = &ret[i + 1];

	ret[size - 1].next = NULL;

	return ret;
}

GC *gc_init (VM *vm, uint32_t bytes)
{
	GC *ret = malloc (sizeof (*ret));

	if (!ret)
		return NULL;

	ret->free = gc_init_pool (bytes / sizeof (*ret->free));

	if (!ret->free) {
		free (ret);
		return NULL;
	}

	ret->used = NULL;
	ret->num_nodes = bytes / sizeof (*ret->free);
	ret->vm = vm;

	return ret;
}

Value *gc_resize (GC *gc)
{
	gc->free = gc_init_pool (gc->num_nodes);
	printf ("gc: allocated %u more nodes\n", gc->num_nodes);
	gc->num_nodes *= 2;

	return gc->free;
}

void gc_scan (Value *v)
{
	if (!v || v->marked)
		return;

	v->marked = 1;

	if (v->type == VAL_PAIR) {
		gc_scan (v->data.pair.a);
		gc_scan (v->data.pair.b);
	}
	else if (v->type == VAL_CLSR) {
		int i;
		for (i = 0; i < v->data.cl->upvals_num; i++)
			gc_scan (v->data.cl->upvals[i]);
	}
}

Value *gc_collect (GC *gc)
{
	int i;
	Value *vit = gc->used, *prev, *next;
	Scope *sit = gc->vm->sc;

	printf ("gc: started cycle\n");

	// unmark every used value
	while (vit) {
		vit->marked = 0;
		vit = vit->next;
	}

	// traverse the stack
	for (i = 0; gc->vm->stack[i]; i++)
		gc_scan (gc->vm->stack[i]);

	// go through each scope's variables and mark each one that is still reachable
	while (sit) {
		for (i = 0; i < sit->vars_num; i++)
			gc_scan (sit->vars[i]);

		sit = sit->next;
	}

	vit = gc->used;
	prev = NULL;
	while (vit) {
		if (vit->marked) { // still reachable
			prev = vit;
			vit = vit->next;
		}
		else {
			printf ("gc: freeing 0x%08lx\n", vit);

			next = vit->next;
			vit->next = gc->free;
			gc->free = vit;

			if (prev)
				prev->next = next;
			else
				gc->used = next;

			vit = next;
		}
	}

	return gc->free;
}

Value *gc_alloc (GC *gc)
{
	Value *ret;

	if (!gc)
		return NULL;

	if (!gc->free && !gc_collect (gc) && !gc_resize (gc)) {
		error ("out of memory\n");
		return NULL;
	}

	ret = gc->free;

	gc->free = ret->next;
	ret->next = gc->used;
	gc->used = ret;

	return ret;
}
