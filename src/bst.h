#ifndef BST_H__
#define BST_H__

typedef struct BSTNode {
	const char *key;
	void *val;
	uint32_t p;
	struct BSTNode *right, *left;
} BSTNode;

BSTNode *bst_insert (BSTNode *node, const char *key, void *val);
BSTNode *bst_merge (BSTNode *a, BSTNode *b);
BSTNode *bst_delete (BSTNode *node, const char *key, BSTNode **rem);
void *bst_find (BSTNode *node, const char *key);

int bst_depth (BSTNode *node);
int bst_count (BSTNode *node);

#endif
