#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "bst.h"

typedef BSTNode Node;

static inline uint32_t xor ()
{
	static uint32_t xs = 2098155156;
	xs ^= xs << 13;
	xs ^= xs >> 17;
	xs ^= xs << 5;
	return xs;
}

static Node *bst_new_node (const char *key, void *val)
{
	Node *ret = malloc (sizeof (Node));

	if (!ret)
		return NULL;

	ret->key = key;
	ret->val = val;
	ret->p = xor ();
	ret->right = ret->left = NULL;

	return ret;
}

// rotates node to be the left child of its right child
static inline Node *bst_rotate_left (Node *node)
{
	Node *child = node->right;
	node->right = child->left;
	child->left = node;

	return child;
}

// and vice-versa...
static inline Node *bst_rotate_right (Node *node)
{
	Node *child = node->left;
	node->left = child->right;
	child->right = node;

	return child;
}

Node *bst_insert (Node *node, const char *key, void *val)
{
	int cmp;

	if (!node)
		return bst_new_node (key, val);

	cmp = strcmp (key, node->key);

	if (cmp > 0) {
		node->right = bst_insert (node->right, key, val);
		if (node->right->p < node->p)
			return bst_rotate_left (node);
	}
	else if (cmp < 0) {
		node->left = bst_insert (node->left, key, val);
		if (node->left->p < node->p)
			return bst_rotate_right (node);
	}

	return node;
}

Node *bst_merge (Node *a, Node *b)
{
	if (!a)
		return b;

	if (!b)
		return a;

	if (a->p < b->p) {
		if (strcmp (a->key, b->key) > 0)
			a->left = bst_merge (a->left, b);
		else
			a->right = bst_merge (a->right, b);

		return a;
	}
	else {
		if (strcmp (b->key, a->key) > 0)
			b->left = bst_merge (b->left, a);
		else
			b->right = bst_merge (b->right, a);

		return b;
	}
}

Node *bst_delete (Node *node, const char *key, Node **rem)
{
	int cmp;

	if (!node)
		return NULL;

	cmp = strcmp (key, node->key);

	if (cmp > 0)
		node->right = bst_delete (node->right, key, rem);
	else if (cmp < 0)
		node->left = bst_delete (node->left, key, rem);
	else if (cmp == 0) {
		*rem = node;
		return bst_merge (node->left, node->right);
	}

	return node;
}

void *bst_find (Node *node, const char *key)
{
	int cmp;

	if (!node)
		return NULL;

	cmp = strcmp (key, node->key);

	if (cmp > 0)
		return bst_find (node->right, key);
	else if (cmp < 0)
		return bst_find (node->left, key);

	return node->val;
}

// some debug stuff:

#define avg(a,b) ((a + b) / 2)
#define max(a,b) ((a > b) ? a : b)
#define min(a,b) ((a < b) ? a : b)
int bst_depth (Node *node)
{
	int rd, ld;

	if (!node)
		return 0;

	rd = bst_depth (node->right);
	ld = bst_depth (node->left);

	return 1 + max (rd, ld);
}

int bst_count (Node *node)
{
	if (!node)
		return 0;

	return 1 + bst_count (node->right) + bst_count (node->left);
}

#if 0
void bst_check (Node *node)
{
	if (!node) return;

	// report if priority is out of order
	if ((node->right && node->p > node->right->p) || (node->left && node->p > node->left->p))
		printf ("priority out of order at %lx\n", node);

	// and check key order
	if (node->left && strcmp (node->key, node->left->key) < 0)
		printf ("left node out of order for %lx\n", node);
	if (node->right && strcmp (node->key, node->right->key) > 0)
		printf ("right node out of order for %lx (%s, %s)\n", node, node->key, node->right->key);

	bst_check (node->right);
	bst_check (node->left);
}
#endif
