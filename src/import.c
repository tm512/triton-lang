#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>

#include "array.h"
#include "vm.h"
#include "load.h"

static const char *tn_import_path_str = NULL;
array_def (tn_import_path, const char*);

int tn_import_set_path (char *path)
{
	if (tn_import_path_str) {
		free ((void*)tn_import_path_str);
		free (tn_import_path);
	}

	path = strdup (path);

	if (!path)
		return 1;

	array_init (tn_import_path);

	if (!tn_import_path) {
		free (path);
		return 1;
	}

	tn_import_path_str = path;

	do {
		if (*path == ':') {
			*path = '\0';
			path++;
		}

		array_add (tn_import_path, path);
	} while ((path = strchr (path, ':')));

//	int i;
//	for (i = 0; i < tn_import_path_num; i++)
//		printf ("path: %s\n", tn_import_path[i]);

	return 0;
}

struct tn_chunk *tn_import_load (const char *name, const char *from)
{
	int i;
	char path[PATH_MAX];
	struct tn_chunk *ret = NULL;

	if (from)
		chdir (dirname (from));

	for (i = 0; i < tn_import_path_num && !ret; i++) {
		snprintf (path, PATH_MAX, "%s/%s.tn", tn_import_path[i], name);
//		printf ("trying %s\n", path);

		ret = tn_load_file (path, NULL);

		if (ret)
			return ret;
	}

	return NULL;
}
