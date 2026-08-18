#ifndef VITA_ROMM_UTIL_H
#define VITA_ROMM_UTIL_H

#include "app_types.h"

void trim_line(char *text);
bool starts_with(const char *text, const char *prefix);
void copy_value(char *dst, size_t dst_size, const char *value);
void lowercase_ascii(char *text);
bool ends_with_ci(const char *text, const char *suffix);
void sanitize_filename(char *text);
const char *basename_ptr(const char *path);
bool path_has_traversal(const char *path);

#endif
