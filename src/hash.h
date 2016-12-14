#ifndef HASH_H_
#define HASH_H_

#include <stdint.h>

struct tn_hash {
	int load, size;
	struct tn_hash_entry {
		const char *key;
		void *data;
	} *entries;
};

uint32_t tn_hash_string (const char *s);
struct tn_hash *tn_hash_new (int init_size);
int tn_hash_resize (struct tn_hash *hash);
int tn_hash_insert (struct tn_hash *hash, const char *key, void *data);
void *tn_hash_search_ref (struct tn_hash *hash, const char *key);
void *tn_hash_search (struct tn_hash *hash, const char *key);

#endif
