#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "bst.h"

static inline uint32_t xor ()
{
	static uint32_t xs = 2098155156;
	xs ^= xs << 13;
	xs ^= xs >> 17;
	xs ^= xs << 5;
	return xs;
}

static struct tn_bst *tn_bst_new_node (const char *key, void *val)
{
	struct tn_bst *ret = malloc (sizeof (struct tn_bst));

	if (!ret)
		return NULL;

	ret->key = key;
	ret->val = val;
	ret->p = xor ();
	ret->right = ret->left = NULL;

	return ret;
}

// rotates node to be the left child of its right child
static inline struct tn_bst *tn_bst_rotate_left (struct tn_bst *node)
{
	struct tn_bst *child = node->right;
	node->right = child->left;
	child->left = node;

	return child;
}

// and vice-versa...
static inline struct tn_bst *tn_bst_rotate_right (struct tn_bst *node)
{
	struct tn_bst *child = node->left;
	node->left = child->right;
	child->right = node;

	return child;
}

struct tn_bst *tn_bst_insert (struct tn_bst *node, const char *key, void *val)
{
	int cmp;

	if (!node)
		return tn_bst_new_node (key, val);

	cmp = strcmp (key, node->key);

	if (cmp > 0) {
		node->right = tn_bst_insert (node->right, key, val);
		if (node->right->p < node->p)
			return tn_bst_rotate_left (node);
	}
	else if (cmp < 0) {
		node->left = tn_bst_insert (node->left, key, val);
		if (node->left->p < node->p)
			return tn_bst_rotate_right (node);
	}
	else
		node->val = val;

	return node;
}

struct tn_bst *tn_bst_merge (struct tn_bst *a, struct tn_bst *b)
{
	if (!a)
		return b;

	if (!b)
		return a;

	if (a->p < b->p) {
		if (strcmp (a->key, b->key) > 0)
			a->left = tn_bst_merge (a->left, b);
		else
			a->right = tn_bst_merge (a->right, b);

		return a;
	}
	else {
		if (strcmp (b->key, a->key) > 0)
			b->left = tn_bst_merge (b->left, a);
		else
			b->right = tn_bst_merge (b->right, a);

		return b;
	}
}

struct tn_bst *tn_bst_delete (struct tn_bst *node, const char *key, struct tn_bst **rem)
{
	int cmp;

	if (!node)
		return NULL;

	cmp = strcmp (key, node->key);

	if (cmp > 0)
		node->right = tn_bst_delete (node->right, key, rem);
	else if (cmp < 0)
		node->left = tn_bst_delete (node->left, key, rem);
	else {
		*rem = node;
		return tn_bst_merge (node->left, node->right);
	}

	return node;
}

void *tn_bst_find_ref (struct tn_bst *node, const char *key)
{
	int cmp;

	if (!node)
		return NULL;

	cmp = strcmp (key, node->key);

	if (cmp > 0)
		return tn_bst_find_ref (node->right, key);
	else if (cmp < 0)
		return tn_bst_find_ref (node->left, key);

	return &node->val;
}

void *tn_bst_find (struct tn_bst *node, const char *key)
{
	void **ref = tn_bst_find_ref (node, key);
	return ref ? *ref : NULL;
}

// some debug stuff:

#define avg(a,b) ((a + b) / 2)
#define max(a,b) ((a > b) ? a : b)
#define min(a,b) ((a < b) ? a : b)
int tn_bst_depth (struct tn_bst *node)
{
	int rd, ld;

	if (!node)
		return 0;

	rd = tn_bst_depth (node->right);
	ld = tn_bst_depth (node->left);

	return 1 + max (rd, ld);
}

int tn_bst_count (struct tn_bst *node)
{
	if (!node)
		return 0;

	return 1 + tn_bst_count (node->right) + tn_bst_count (node->left);
}
