#ifndef IMPORT_H__
#define IMPORT_H__

int tn_import_set_path (char *path);
struct tn_chunk *tn_import_load (const char *name, const char *from);

#endif
