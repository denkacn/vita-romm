#ifndef VITA_ROMM_VITA_PATHS_H
#define VITA_ROMM_VITA_PATHS_H

#include "app_types.h"

void vita_path_join(char *dst, size_t dst_size, const char *root, const char *name);
void vita_path_join3(char *dst, size_t dst_size, const char *root, const char *folder, const char *name);
void vita_path_part(char *dst, size_t dst_size, const char *path);

#endif
