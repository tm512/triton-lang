#ifndef GEN_H__
#define GEN_H__

struct tn_chunk;
struct tn_chunk_vars;
struct tn_expr_data_fn;
struct tn_chunk *tn_gen_compile (struct tn_expr *ex, struct tn_expr_data_fn *fn,
                                 struct tn_chunk *next, struct tn_chunk_vars *vars);

#endif
