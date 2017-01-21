#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "hash_rand.h" // for rlut
#include "hash.h"

uint32_t tn_hash_string (const char *s)
{
	uint32_t ret = 0;

	while (*s) {
		ret ^= rlut[*(s++)];
		ret = (ret << 7) | (ret >> 25);
	}

	return ret;
}

struct tn_hash *tn_hash_new (int init_size)
{
	struct tn_hash *ret = malloc (sizeof (*ret));

	if (!ret)
		return NULL;

	ret->load = 0;
	ret->size = init_size;

	ret->entries = malloc (init_size * sizeof (*ret->entries));

	if (!ret->entries) {
		free (ret);
		return NULL;
	}

	memset (ret->entries, 0, init_size * sizeof (*ret->entries));

	return ret;
}

int tn_hash_resize (struct tn_hash *hash)
{
	struct tn_hash_entry *old = hash->entries;
	int oldsize = hash->size;
	int i;

	hash->load = 0;
	hash->size *= 2;
	hash->entries = malloc (hash->size * sizeof (*hash->entries));

	if (!hash->entries) {
		error ("malloc failed\n");
		free (old);
		return 1;
	}

	memset (hash->entries, 0, hash->size * sizeof (*hash->entries));

	for (i = 0; i < oldsize; i++) {
		if (old[i].key && tn_hash_insert (hash, old[i].key, old[i].data)) {
			error ("rehashing failed after resizing\n");
			free (old);
			return 1;
		}
	}

	return 0;
}

int tn_hash_insert (struct tn_hash *hash, const char *key, void *data)
{
	uint32_t keyval;

	if (hash->load > hash->size * 0.75 && tn_hash_resize (hash)) {
		error ("failed to resize hash table\n");
		return 1;
	}

	keyval = tn_hash_string (key) % hash->size;

	while (hash->entries[keyval].key) {
		keyval++;
		keyval %= hash->size;
	}

//	printf ("%s - %u/%u\n", key, keyval, hash->size);

	hash->entries[keyval].key = key;
	hash->entries[keyval].data = data;

	hash->load++;

	return 0;
}

void *tn_hash_search_ref (struct tn_hash *hash, const char *key)
{
	uint32_t keyval = tn_hash_string (key) % hash->size;

	while (hash->entries[keyval].key) {
	//	printf ("checking %s - %u/%u\n", hash->entries[keyval].key, keyval, hash->size);
		if (!strcmp (key, hash->entries[keyval].key))
			return &hash->entries[keyval].data;
		else {
			keyval++;
			keyval %= hash->size;
		}
	}

	return NULL;
}

void *tn_hash_search (struct tn_hash *hash, const char *key)
{
	void **ref = tn_hash_search_ref (hash, key);
	return ref ? *ref : NULL;
}

static char *keygen ()
{
	static char key[] = "aaaa";
	char *ret;
	int pos = strlen (key) - 1;

	while (pos >= 0) {
		if (key[pos] > 'z') {
			key[pos] = 'a';
			key[--pos]++;
			continue;
		}

		ret = strdup (key);
		key[pos]++;
		return ret;
	}

	return NULL;
}
