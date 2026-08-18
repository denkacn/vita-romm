#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include "app_types.h"

const char *find_json_object_end(const char *start);
const char *find_json_array_end(const char *start);
bool find_json_array_field_partial(const char *json, const char *field, const char **array_start, const char **array_end);

bool extract_string(const char *start, const char *end, const char *field, char *dst, size_t dst_size);
bool extract_first_string(const char *start, const char *end, const char **fields, int field_count, char *dst, size_t dst_size);
bool extract_int(const char *start, const char *end, const char *field, int *value);
bool extract_int64(const char *start, const char *end, const char *field, long long *value);
long long extract_size_bytes(const char *start, const char *end);

bool object_contains(const char *start, const char *end, const char *needle);
bool object_has_platform_id(const char *start, const char *end, int platform_id);

#endif
