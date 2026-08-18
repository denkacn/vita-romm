#include "vita_paths.h"

void vita_path_join(char *dst, size_t dst_size, const char *root, const char *name) {
    if (!dst || dst_size == 0) return;
    snprintf(dst, dst_size, "%s/%s", root ? root : "", name ? name : "");
}

void vita_path_join3(char *dst, size_t dst_size, const char *root, const char *folder, const char *name) {
    if (!dst || dst_size == 0) return;
    snprintf(dst, dst_size, "%s/%s/%s", root ? root : "", folder ? folder : "", name ? name : "");
}

void vita_path_part(char *dst, size_t dst_size, const char *path) {
    if (!dst || dst_size == 0) return;
    snprintf(dst, dst_size, "%s.part", path ? path : "");
}
