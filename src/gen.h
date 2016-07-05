#ifndef GEN_H__
#define GEN_H__

struct tn_chunk;
struct tn_bst;
struct tn_chunk *tn_gen_compile (struct tn_expr *ex, const char *name, const char **args,
                                 int argn, struct tn_chunk *next, struct tn_bst *vartree);

#endif
