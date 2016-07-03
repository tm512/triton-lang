#ifndef BST_H__
#define BST_H__

struct tn_bst {
	const char *key;
	void *val;
	uint32_t p;
	struct tn_bst *right, *left;
};

struct tn_bst *tn_bst_insert (struct tn_bst *node, const char *key, void *val);
struct tn_bst *tn_bst_merge (struct tn_bst *a, struct tn_bst *b);
struct tn_bst *tn_bst_delete (struct tn_bst *node, const char *key, struct tn_bst **rem);
void *tn_bst_find (struct tn_bst *node, const char *key);

int tn_bst_depth (struct tn_bst *node);
int tn_bst_count (struct tn_bst *node);

#endif
