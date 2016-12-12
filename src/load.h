#ifndef LOAD_H__
#define LOAD_H__

struct tn_chunk;
struct tn_chunk_vars;
struct tn_token;

struct tn_chunk *tn_load_tokens (struct tn_token *tok, struct tn_chunk_vars *vars);
struct tn_chunk *tn_load_file (const char *path, struct tn_chunk_vars *vars);
struct tn_chunk *tn_load_string (const char *str, struct tn_chunk_vars *vars);

#endif
