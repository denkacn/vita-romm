#include "json_utils.h"
#include "util.h"

int extract_collection_count(const char *json) {
    int value = 0;
    const char *end = json + strlen(json);
    if (extract_int(json, end, "total", &value) && value >= 0) return value;
    if (extract_int(json, end, "count", &value) && value >= 0) return value;
    if (extract_int(json, end, "total_count", &value) && value >= 0) return value;

    int objects = 0;
    const char *tmp = json;
    while ((tmp = strstr(tmp, "\"id\"")) != NULL) {
        objects++;
        tmp += 4;
    }
    return objects;
}

bool find_cover_url(const char *start, const char *end, char *dst, size_t dst_size) {
    const char *fields[] = { "path_cover_large", "path_cover_small", "url_cover", "cover_url", "image_url", "thumbnail_url", "thumbnail", "cover", "image", "path_cover_l", "path_cover_s", "path_cover" };
    for (int i = 0; i < 12; i++) {
        if (extract_string(start, end, fields[i], dst, dst_size) && dst[0]) return true;
    }
    return false;
}

int parse_rom_items(const char *json, const char *platform_slug, int expected_platform_id, RomItem *roms, int max_roms) {
    int count = 0;
    const char *scan_start = json;
    const char *scan_end = json + strlen(json);
    const char *array_start = NULL;
    const char *array_end = NULL;
    if (find_json_array_field_partial(json, "items", &array_start, &array_end) ||
        find_json_array_field_partial(json, "results", &array_start, &array_end) ||
        find_json_array_field_partial(json, "data", &array_start, &array_end)) {
        scan_start = array_start;
        scan_end = array_end;
    } else {
        const char *first_array = strchr(json, '[');
        if (first_array) {
            const char *first_array_end = find_json_array_end(first_array);
            if (first_array_end) {
                scan_start = first_array;
                scan_end = first_array_end;
            }
        }
    }

    const char *cur = scan_start;
    while (count < max_roms && cur < scan_end && (cur = strchr(cur, '{')) != NULL && cur < scan_end) {
        const char *end = find_json_object_end(cur);
        if (!end || end > scan_end) break;
        RomItem *rom = &roms[count];
        memset(rom, 0, sizeof(*rom));
        int id = -1;
        extract_int(cur, end, "id", &id);
        if (expected_platform_id >= 0 && !object_has_platform_id(cur, end, expected_platform_id)) {
            cur = end + 1;
            continue;
        }
        const char *name_fields[] = { "name", "fs_name", "file_name", "filename", "title" };
        if (id < 0 || !extract_first_string(cur, end, name_fields, 5, rom->name, sizeof(rom->name))) {
            cur = end + 1;
            continue;
        }
        rom->id = id;
        rom->size_bytes = extract_size_bytes(cur, end);
        const char *file_fields[] = { "fs_name", "file_name", "filename", "path", "name" };
        if (!extract_first_string(cur, end, file_fields, 5, rom->file_name, sizeof(rom->file_name))) {
            char fallback_name[180];
            snprintf(fallback_name, sizeof(fallback_name), "%.150s.zip", rom->name);
            snprintf(rom->file_name, sizeof(rom->file_name), "%s", fallback_name);
            sanitize_filename(rom->file_name);
        }
        find_cover_url(cur, end, rom->cover_url, sizeof(rom->cover_url));
        snprintf(rom->cover_path, sizeof(rom->cover_path), "%s/%s_%d.png", POSTER_CACHE_DIR, platform_slug, rom->id);
        count++;
        cur = end + 1;
    }
    return count;
}

int count_roms_for_platform(const char *json, int expected_platform_id) {
    int count = 0;
    const char *scan_start = json;
    const char *scan_end = json + strlen(json);
    const char *array_start = NULL;
    const char *array_end = NULL;
    if (find_json_array_field_partial(json, "items", &array_start, &array_end) ||
        find_json_array_field_partial(json, "results", &array_start, &array_end) ||
        find_json_array_field_partial(json, "data", &array_start, &array_end)) {
        scan_start = array_start;
        scan_end = array_end;
    } else {
        const char *first_array = strchr(json, '[');
        if (first_array) {
            const char *first_array_end = find_json_array_end(first_array);
            if (first_array_end) {
                scan_start = first_array;
                scan_end = first_array_end;
            }
        }
    }

    const char *cur = scan_start;
    while (cur < scan_end && (cur = strchr(cur, '{')) != NULL && cur < scan_end) {
        const char *end = find_json_object_end(cur);
        if (!end || end > scan_end) break;
        int id = -1;
        if (extract_int(cur, end, "id", &id) && id >= 0 && object_has_platform_id(cur, end, expected_platform_id)) {
            count++;
        }
        cur = end + 1;
    }
    return count;
}

int count_rom_objects(const char *json) {
    int count = 0;
    const char *scan_start = json;
    const char *scan_end = json + strlen(json);
    const char *array_start = NULL;
    const char *array_end = NULL;
    if (find_json_array_field_partial(json, "items", &array_start, &array_end) ||
        find_json_array_field_partial(json, "results", &array_start, &array_end) ||
        find_json_array_field_partial(json, "data", &array_start, &array_end)) {
        scan_start = array_start;
        scan_end = array_end;
    } else {
        const char *first_array = strchr(json, '[');
        if (first_array) {
            const char *first_array_end = find_json_array_end(first_array);
            if (first_array_end) {
                scan_start = first_array;
                scan_end = first_array_end;
            }
        }
    }

    const char *cur = scan_start;
    while (cur < scan_end && (cur = strchr(cur, '{')) != NULL && cur < scan_end) {
        const char *end = find_json_object_end(cur);
        if (!end || end > scan_end) break;
        int id = -1;
        if (extract_int(cur, end, "id", &id) && id >= 0) count++;
        cur = end + 1;
    }
    return count;
}
