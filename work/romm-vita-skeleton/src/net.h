#ifndef VITA_ROMM_NET_H
#define VITA_ROMM_NET_H

#include "app_types.h"

void init_network(AppState *state);
void refresh_network(AppState *state);
CURLcode http_get(AppState *state, const char *url, char *response, long *code_out);
CURLcode http_get_reuse(AppState *state, CURL *curl, const char *url, char *response, long *code_out);
CURLcode http_download(AppState *state, const char *url, const char *path, long *code_out);
CURLcode http_download_quiet(AppState *state, const char *url, const char *path, long *code_out);

#endif
