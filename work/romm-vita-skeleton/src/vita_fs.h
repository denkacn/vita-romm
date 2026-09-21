#ifndef VITA_ROMM_VITA_FS_H
#define VITA_ROMM_VITA_FS_H

#include "app_types.h"

void mkdir_parent_for_file(const char *path);
int remove_path_recursive(const char *path);
int copy_file(const char *src, const char *dst);
bool usable_file_exists(const char *path);
bool dir_exists(const char *path);
void redraw_now(const AppState *state);
void append_install_log(const char *line);
void log_event(const char *event, const char *detail);
void debug_log_event(const AppState *state, const char *event, const char *detail);

#endif
