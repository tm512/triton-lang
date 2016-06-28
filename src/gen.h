#ifndef GEN_H__
#define GEN_H__

struct Chunk;
struct Chunk *gen_compile (Expr *ex, const char *name, const char **args, int argn);

#endif
