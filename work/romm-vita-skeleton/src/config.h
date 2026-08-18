#ifndef VITA_ROMM_CONFIG_H
#define VITA_ROMM_CONFIG_H

#include "app_types.h"

void init_defaults(AppState *state);
void ensure_config(AppState *state);
void set_platform_path(AppState *state, const char *slug, const char *path);
const char *get_platform_path_config(const AppState *state, const char *slug);
void build_api_url(char *dst, size_t dst_size, const AppState *state, const char *path);
void escape_url_spaces(char *url, size_t url_size);
void url_encode(char *dst, size_t dst_size, const char *src);

#endif
