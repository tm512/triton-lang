#ifndef ARRAY_H__
#define ARRAY_H__

// simple dynamic array implementation

#define array_def(NAME, TYPE) int NAME##_max, NAME##_num; TYPE *NAME

#define array_init(AR) \
do { \
	AR = malloc (sizeof (*AR)); \
	AR##_max = 1; \
	AR##_num = 0; \
} while (0)

#define array_add_at(AR, DATA, IDX) \
do { \
	int idx = (IDX); \
	__typeof__ (*AR) data = (DATA); \
	if (idx >= AR##_max) { \
		if (idx > (AR##_max *= 2)) \
			AR##_max = idx; \
		AR = realloc (AR, AR##_max * sizeof (*AR)); \
	} \
	if (AR) \
		AR[idx] = data; \
	if (idx >= AR##_num) \
		AR##_num = idx + 1; \
} while (0)

#define array_add(AR, DATA) array_add_at(AR, DATA, AR##_num)

#endif
