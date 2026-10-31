#include "json_utils.h"

const char *find_json_object_end(const char *start) {
    int depth = 0;
    bool in_string = false;
    bool escape = false;
    for (const char *p = start; *p; p++) {
        if (escape) {
            escape = false;
            continue;
        }
        if (in_string && *p == '\\') {
            escape = true;
            continue;
        }
        if (*p == '"') {
            in_string = !in_string;
            continue;
        }
        if (in_string) continue;
        if (*p == '{') depth++;
        if (*p == '}') {
            depth--;
            if (depth == 0) return p;
        }
    }
    return NULL;
}

const char *find_json_array_end(const char *start) {
    int depth = 0;
    bool in_string = false;
    bool escape = false;
    for (const char *p = start; *p; p++) {
        if (escape) {
            escape = false;
            continue;
        }
        if (in_string && *p == '\\') {
            escape = true;
            continue;
        }
        if (*p == '"') {
            in_string = !in_string;
            continue;
        }
        if (in_string) continue;
        if (*p == '[') depth++;
        if (*p == ']') {
            depth--;
            if (depth == 0) return p;
        }
    }
    return NULL;
}

bool find_json_array_field_partial(const char *json, const char *field, const char **array_start, const char **array_end) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", field);
    const char *key = strstr(json, pat);
    if (!key) return false;
    const char *colon = strchr(key + strlen(pat), ':');
    if (!colon) return false;
    const char *array = strchr(colon, '[');
    if (!array) return false;
    const char *end = find_json_array_end(array);
    if (!end) return false;
    *array_start = array;
    *array_end = end;
    return true;
}

bool extract_string(const char *start, const char *end, const char *field, char *dst, size_t dst_size) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", field);
    const char *key = strstr(start, pat);
    if (!key || key >= end) return false;
    const char *colon = strchr(key + strlen(pat), ':');
    if (!colon || colon >= end) return false;
    const char *q = colon + 1;
    while (q < end && (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n')) q++;
    if (q >= end || *q != '"') return false;
    q++;
    const char *e = q;
    bool esc = false;
    while (e < end) {
        if (esc) esc = false;
        else if (*e == '\\') esc = true;
        else if (*e == '"') break;
        e++;
    }
    if (e >= end) return false;
    size_t n = (size_t)(e - q);
    if (n >= dst_size) n = dst_size - 1;
    memcpy(dst, q, n);
    dst[n] = '\0';
    return true;
}

bool extract_first_string(const char *start, const char *end, const char **fields, int field_count, char *dst, size_t dst_size) {
    for (int i = 0; i < field_count; i++) {
        if (extract_string(start, end, fields[i], dst, dst_size) && dst[0]) return true;
    }
    return false;
}

bool extract_int(const char *start, const char *end, const char *field, int *value) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", field);
    const char *key = strstr(start, pat);
    if (!key || key >= end) return false;
    const char *colon = strchr(key + strlen(pat), ':');
    if (!colon || colon >= end) return false;
    *value = atoi(colon + 1);
    return true;
}

bool extract_int64(const char *start, const char *end, const char *field, long long *value) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", field);
    const char *key = strstr(start, pat);
    if (!key || key >= end) return false;
    const char *colon = strchr(key + strlen(pat), ':');
    if (!colon || colon >= end) return false;
    const char *p = colon + 1;
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
    if (p >= end || ((*p < '0' || *p > '9') && *p != '-')) return false;
    *value = atoll(p);
    return true;
}

long long extract_size_bytes(const char *start, const char *end) {
    const char *fields[] = {
        "fs_size_bytes", "file_size_bytes", "size_bytes", "fs_size", "file_size",
        "filesize", "rom_size", "total_size", "size"
    };
    long long value = 0;
    for (int i = 0; i < 9; i++) {
        if (extract_int64(start, end, fields[i], &value) && value > 0) return value;
    }
    return 0;
}

bool object_contains(const char *start, const char *end, const char *needle) {
    size_t needle_len = strlen(needle);
    for (const char *p = start; p && p < end; p++) {
        p = strstr(p, needle);
        if (!p || p >= end) return false;
        if (p + needle_len <= end) return true;
    }
    return false;
}

bool object_has_platform_id(const char *start, const char *end, int platform_id) {
    int id = -1;
    if (extract_int(start, end, "platform_id", &id) && id == platform_id) return true;
    if (extract_int(start, end, "platform", &id) && id == platform_id) return true;

    const char *key = strstr(start, "\"platform\"");
    if (key && key < end) {
        const char *platform_end = find_json_object_end(key);
        if (platform_end && platform_end <= end && extract_int(key, platform_end, "id", &id) && id == platform_id) return true;
    }
    return false;
}
