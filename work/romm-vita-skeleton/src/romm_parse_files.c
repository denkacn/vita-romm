#include "json_utils.h"
#include "romm_parser.h"
#include "util.h"

static bool string_has_file_extension(const char *text) {
    return ends_with_ci(text, ".bin") || ends_with_ci(text, ".cue") ||
        ends_with_ci(text, ".pbp") || ends_with_ci(text, ".iso") ||
        ends_with_ci(text, ".cso") || ends_with_ci(text, ".gba") ||
        ends_with_ci(text, ".gbc") || ends_with_ci(text, ".gb") ||
        ends_with_ci(text, ".nes") || ends_with_ci(text, ".sfc") ||
        ends_with_ci(text, ".smc") || ends_with_ci(text, ".md") ||
        ends_with_ci(text, ".gen") || ends_with_ci(text, ".z64") ||
        ends_with_ci(text, ".n64") || ends_with_ci(text, ".v64");
}

static bool object_has_file_extension(const char *start, const char *end) {
    return object_contains(start, end, ".bin") || object_contains(start, end, ".cue") ||
        object_contains(start, end, ".pbp") || object_contains(start, end, ".iso") ||
        object_contains(start, end, ".cso") || object_contains(start, end, ".gba") ||
        object_contains(start, end, ".gbc") || object_contains(start, end, ".gb") ||
        object_contains(start, end, ".nes") || object_contains(start, end, ".sfc") ||
        object_contains(start, end, ".smc") || object_contains(start, end, ".md") ||
        object_contains(start, end, ".gen") || object_contains(start, end, ".z64") ||
        object_contains(start, end, ".n64") || object_contains(start, end, ".v64");
}

static bool extract_file_name_candidate(const char *start, const char *end, char *dst, size_t dst_size) {
    const char *fields[] = { "file_name", "filename", "fs_name", "path", "fs_path", "file_path", "name" };
    char value[360];
    for (int i = 0; i < 7; i++) {
        if (extract_string(start, end, fields[i], value, sizeof(value)) && string_has_file_extension(value)) {
            snprintf(dst, dst_size, "%s", value);
            return true;
        }
    }
    return false;
}

static bool rom_file_exists(const RomFileItem *files, int count, const char *name) {
    for (int i = 0; i < count; i++) {
        if (strcmp(files[i].name, name) == 0) return true;
    }
    return false;
}

static int parse_rom_file_objects(const char *start, const char *end_limit, RomFileItem *files, int count, int max_files) {
    const char *cur = start;
    while (count < max_files && cur < end_limit && (cur = strchr(cur, '{')) != NULL && cur < end_limit) {
        const char *end = find_json_object_end(cur);
        if (!end || end > end_limit) break;
        const char *child = strchr(cur + 1, '{');
        if (child && child < end) {
            cur++;
            continue;
        }
        if (!object_has_file_extension(cur, end)) {
            cur = end + 1;
            continue;
        }

        char name[360];
        if (!extract_file_name_candidate(cur, end, name, sizeof(name))) {
            cur = end + 1;
            continue;
        }
        if (rom_file_exists(files, count, name)) {
            cur = end + 1;
            continue;
        }

        RomFileItem *item = &files[count];
        memset(item, 0, sizeof(*item));
        item->id = -1;
        extract_int(cur, end, "id", &item->id);
        copy_value(item->name, sizeof(item->name), name);
        const char *url_fields[] = { "download_url", "content_url", "path_download", "download_path", "url" };
        extract_first_string(cur, end, url_fields, 5, item->url, sizeof(item->url));
        item->size_bytes = extract_size_bytes(cur, end);
        count++;
        cur = end + 1;
    }
    return count;
}

static long long sum_file_sizes(const RomFileItem *files, int count) {
    long long total = 0;
    bool saw_file_size = false;
    for (int i = 0; i < count; i++) {
        if (files[i].size_bytes > 0) {
            total += files[i].size_bytes;
            saw_file_size = true;
        }
    }
    return saw_file_size ? total : 0;
}

long long parse_rom_total_size(const char *json) {
    RomFileItem files[32];
    int count = parse_rom_files(json, files, 32);
    long long total = sum_file_sizes(files, count);
    if (total > 0) return total;
    return extract_size_bytes(json, json + strlen(json));
}

int parse_rom_files(const char *json, RomFileItem *files, int max_files) {
    int count = 0;
    const char *array_fields[] = { "files", "rom_files", "romFiles", "siblings", "content", "media" };
    for (int i = 0; i < 6 && count < max_files; i++) {
        const char *array_start = NULL;
        const char *array_end = NULL;
        if (find_json_array_field_partial(json, array_fields[i], &array_start, &array_end)) {
            count = parse_rom_file_objects(array_start, array_end, files, count, max_files);
        }
    }
    if (count <= 0) {
        count = parse_rom_file_objects(json, json + strlen(json), files, count, max_files);
    }
    return count;
}
