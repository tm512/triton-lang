#include <stdio.h>
#include <stdlib.h>

#include "error.h"
#include "hash.h"
#include "value.h"
#include "vm.h"
#include "gc.h"

#define printf(...) (0) // shut up
struct tn_value *tn_gc_init_pool (uint32_t size)
{
	struct tn_value *ret;
	int i;

	ret = malloc (size * sizeof (*ret));

	if (!ret)
		return NULL;

	printf ("gc: allocated %u struct tn_values\n", size);

	for (i = 0; i < size - 1; i++)
		ret[i].next = &ret[i + 1];

	ret[size - 1].next = NULL;

	return ret;
}

struct tn_gc *tn_gc_init (struct tn_vm *vm, uint32_t bytes)
{
	struct tn_gc *ret = malloc (sizeof (*ret));

	if (!ret)
		return NULL;

	ret->free = tn_gc_init_pool (bytes / sizeof (*ret->free));

	if (!ret->free) {
		free (ret);
		return NULL;
	}

	ret->used = NULL;
	ret->num_nodes = bytes / sizeof (*ret->free);
	ret->vm = vm;

	return ret;
}

struct tn_value *tn_gc_resize (struct tn_gc *gc)
{
	gc->free = tn_gc_init_pool (gc->num_nodes);
	printf ("gc: allocated %u more nodes\n", gc->num_nodes);
	gc->num_nodes *= 2;

	return gc->free;
}

void tn_gc_scan (struct tn_value *v)
{
	if (!v || v->flags & GC_MARKED)
		return;

	v->flags |= GC_MARKED;

	if (v->type == VAL_PAIR) {
		tn_gc_scan (v->data.pair.a);
		tn_gc_scan (v->data.pair.b);
	}
	else if (v->type == VAL_CLSR) {
		int i;
		struct tn_scope *sit = v->data.cl->sc;

		while (sit) {
			for (i = 0; i < sit->vars->arr_num; i++)
				tn_gc_scan (sit->vars->arr[i]);

			sit = sit->next;
		}
	}
}

struct tn_value *tn_gc_collect (struct tn_gc *gc)
{
	int i;
	struct tn_value *vit = gc->used, *prev, *next;
	struct tn_scope *sit = gc->vm->sc, *snext;

	printf ("gc: started cycle\n");

	// unmark every used value
	while (vit) {
		vit->flags &= ~GC_MARKED;
		vit = vit->next;
	}

	// scan globals
	for (i = 0; i < gc->vm->globals->size; i++)
		tn_gc_scan (gc->vm->globals->entries[i].data);

	// traverse the stack
	for (i = 0; gc->vm->stack[i]; i++)
		tn_gc_scan (gc->vm->stack[i]);

	// go through each scope's variables and mark each one that is still reachable
	while (sit) {
		for (i = 0; i < sit->vars->arr_num; i++)
			tn_gc_scan (sit->vars->arr[i]);

		sit = sit->gc_next;
	}

	vit = gc->used;
	prev = NULL;
	while (vit) {
		if (vit->flags != 0) {
			printf ("gc: 0x%08lx still reachable (%i)\n", vit, vit->type);
			prev = vit;
			vit = vit->next;
		}
		else {
			printf ("gc: freeing 0x%08lx\n", vit);

			if (vit->type == VAL_CLSR) {
				printf ("gc: closure %lx\n", vit->data.cl);
				sit = vit->data.cl->sc;
				while (sit) {
					snext = sit->next;

					if (--sit->vars->refs == 0) {
						free (sit->vars->arr);
						free (sit->vars);
					}

					free (sit);
					sit = snext;
				}

				free (vit->data.cl);
			}
			else if (vit->type == VAL_STR)
				free (vit->data.s);

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

void tn_gc_preserve (struct tn_value *val)
{
	val->flags |= GC_PRESERVE;
}

void tn_gc_release (struct tn_value *val)
{
	val->flags &= ~GC_PRESERVE;
}

void tn_gc_release_list (struct tn_value *lst)
{
	struct tn_value *it = lst;

	while (it != &nil) {
		tn_gc_release (it->data.pair.a);
		tn_gc_release (it);
		it = it->data.pair.b;
	}
}

struct tn_value *tn_gc_alloc (struct tn_gc *gc)
{
	int i;
	struct tn_value *ret;

	if (!gc)
		return NULL;

	if (!gc->free && !tn_gc_collect (gc) && !tn_gc_resize (gc)) {
		error ("out of memory\n");
		return NULL;
	}

	ret = gc->free;

	gc->free = ret->next;
	ret->next = gc->used;
	gc->used = ret;

	return ret;
}
