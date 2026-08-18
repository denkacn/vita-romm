#include "util.h"

void trim_line(char *text) {
    size_t len = strlen(text);
    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r' || text[len - 1] == ' ' || text[len - 1] == '\t')) {
        text[len - 1] = '\0';
        len--;
    }
}

bool starts_with(const char *text, const char *prefix) {
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

void copy_value(char *dst, size_t dst_size, const char *value) {
    if (dst_size == 0) return;
    snprintf(dst, dst_size, "%s", value);
    trim_line(dst);
}

void lowercase_ascii(char *text) {
    for (char *p = text; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    }
}

bool ends_with_ci(const char *text, const char *suffix) {
    size_t text_len = strlen(text);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > text_len) return false;
    const char *a = text + text_len - suffix_len;
    for (size_t i = 0; i < suffix_len; i++) {
        char ca = a[i];
        char cb = suffix[i];
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return false;
    }
    return true;
}

void sanitize_filename(char *text) {
    for (char *p = text; *p; p++) {
        if (*p == '/' || *p == '\\' || *p == ':' || *p == '*' || *p == '?' || *p == '"' || *p == '<' || *p == '>' || *p == '|') {
            *p = '_';
        }
    }
}

const char *basename_ptr(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *base = slash > backslash ? slash : backslash;
    return base ? base + 1 : path;
}

bool path_has_traversal(const char *path) {
    if (path[0] == '/' || path[0] == '\\') return true;
    if (strchr(path, ':')) return true;
    const char *p = path;
    while (*p) {
        if (p[0] == '.' && p[1] == '.' && (p[2] == '/' || p[2] == '\\' || p[2] == '\0')) return true;
        if ((p[0] == '/' || p[0] == '\\') && p[1] == '.' && p[2] == '.' && (p[3] == '/' || p[3] == '\\' || p[3] == '\0')) return true;
        p++;
    }
    return false;
}
