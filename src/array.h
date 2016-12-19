#ifndef ARRAY_H__
#define ARRAY_H__

// simple dynamic array implementation

#include <string.h>

#define array_def(NAME, TYPE) int NAME##_max, NAME##_num; TYPE *NAME

#define array_init(AR) \
do { \
	AR = malloc (sizeof (*AR)); \
	if (AR) AR[0] = NULL; \
	AR##_max = 1; \
	AR##_num = 0; \
} while (0)


	//printf ("adding %llx to %llx @ %u (max = %u)\n", DATA, AR, idx, AR##_max);
	//	printf ("resizing %llx to %u\n", AR, AR##_max);

#define array_add_at(AR, DATA, IDX) \
do { \
	int idx = (IDX), oldmax = AR##_max; \
	__typeof__ (*AR) data = (DATA); \
	if (idx >= AR##_max) { \
		while (idx >= (AR##_max *= 2)); \
		AR = realloc (AR, AR##_max * sizeof (*AR)); \
		memset (AR + oldmax, 0, (AR##_max - oldmax) * sizeof (*AR)); \
	} \
	if (AR) \
		AR[idx] = data; \
	if (idx >= AR##_num) \
		AR##_num = idx + 1; \
} while (0)

#define array_add(AR, DATA) array_add_at(AR, DATA, AR##_num)

#endif
