static const char *find_json_object_end(const char *start) {
    int depth = 0;
    bool in_string = false;
    bool esc = false;
    for (const char *p = start; *p; p++) {
        char c = *p;
        if (in_string) {
            if (esc) esc = false;
            else if (c == '\\') esc = true;
            else if (c == '"') in_string = false;
        } else {
            if (c == '"') in_string = true;
            else if (c == '{') depth++;
            else if (c == '}' && --depth == 0) return p;
        }
    }
    return NULL;
}

static const char *find_json_array_end(const char *start) {
    int depth = 0;
    bool in_string = false;
    bool esc = false;
    for (const char *p = start; *p; p++) {
        char c = *p;
        if (in_string) {
            if (esc) esc = false;
            else if (c == '\\') esc = true;
            else if (c == '"') in_string = false;
        } else {
            if (c == '"') in_string = true;
            else if (c == '[') depth++;
            else if (c == ']' && --depth == 0) return p;
        }
    }
    return NULL;
}

static bool find_json_array_field_partial(const char *json, const char *field, const char **array_start, const char **array_end) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", field);
    const char *key = strstr(json, pat);
    if (!key) return false;
    const char *colon = strchr(key + strlen(pat), ':');
    if (!colon) return false;
    const char *open = strchr(colon, '[');
    if (!open) return false;
    const char *end = find_json_array_end(open);
    *array_start = open;
    *array_end = end ? end : json + strlen(json);
    return true;
}

static int parse_rom_files(const char *json, RomFileItem *files, int max_files);

static void add_platform_id(PlatformDef *p, int id) {
    if (id < 0) return;
    for (int i = 0; i < p->id_count; i++) {
        if (p->ids[i] == id) return;
    }
    if (p->id_count < MAX_PLATFORM_IDS) p->ids[p->id_count++] = id;
}

static bool extract_string(const char *start, const char *end, const char *field, char *dst, size_t dst_size) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", field);
    const char *key = strstr(start, pat);
    if (!key || key >= end) return false;
    const char *colon = strchr(key + strlen(pat), ':');
    if (!colon || colon >= end) return false;
    const char *q = strchr(colon, '"');
    if (!q || q >= end) return false;
    q++;
    const char *e = q;
    while (e < end && *e != '"') e++;
    if (e >= end) return false;
    size_t n = (size_t)(e - q);
    if (n >= dst_size) n = dst_size - 1;
    memcpy(dst, q, n);
    dst[n] = '\0';
    return true;
}

static bool extract_first_string(const char *start, const char *end, const char **fields, int field_count, char *dst, size_t dst_size) {
    for (int i = 0; i < field_count; i++) {
        if (extract_string(start, end, fields[i], dst, dst_size) && dst[0]) return true;
    }
    return false;
}

static bool extract_int(const char *start, const char *end, const char *field, int *value) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", field);
    const char *key = strstr(start, pat);
    if (!key || key >= end) return false;
    const char *colon = strchr(key + strlen(pat), ':');
    if (!colon || colon >= end) return false;
    *value = atoi(colon + 1);
    return true;
}

static bool extract_int64(const char *start, const char *end, const char *field, long long *value) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", field);
    const char *key = strstr(start, pat);
    if (!key || key >= end) return false;
    const char *colon = strchr(key + strlen(pat), ':');
    if (!colon || colon >= end) return false;
    *value = atoll(colon + 1);
    return true;
}

static long long extract_size_bytes(const char *start, const char *end) {
    const char *fields[] = {
        "file_size_bytes", "size_bytes", "fs_size", "file_size",
        "filesize", "rom_size", "total_size", "size"
    };
    long long value = 0;
    for (int i = 0; i < 8; i++) {
        if (extract_int64(start, end, fields[i], &value) && value > 0) return value;
    }
    return 0;
}

static bool object_contains(const char *start, const char *end, const char *needle) {
    size_t needle_len = strlen(needle);
    for (const char *p = start; p && p < end; p++) {
        p = strstr(p, needle);
        if (!p || p >= end) return false;
        if (p + needle_len <= end) return true;
    }
    return false;
}

static bool object_has_platform_id(const char *start, const char *end, int platform_id) {
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

static void parse_platforms(AppState *state, const char *json) {
    state->platform_count = 0;
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
    while (state->platform_count < MAX_PLATFORMS && cur < scan_end && (cur = strchr(cur, '{')) != NULL && cur < scan_end) {
        const char *end = find_json_object_end(cur);
        if (!end || end > scan_end) break;
        int id = -1;
        char name[96] = "";
        char slug[96] = "";
        char folder[96] = "";
        extract_int(cur, end, "id", &id);
        extract_string(cur, end, "name", name, sizeof(name));
        extract_string(cur, end, "slug", slug, sizeof(slug));
        extract_string(cur, end, "fs_slug", folder, sizeof(folder));
        if (id < 0 || (!name[0] && !slug[0] && !folder[0])) {
            cur = end + 1;
            continue;
        }

        PlatformDef *p = &state->platforms[state->platform_count];
        memset(p, 0, sizeof(*p));
        p->id = id;
        snprintf(p->label, sizeof(p->label), "%s", name[0] ? name : (slug[0] ? slug : folder));
        snprintf(p->slug, sizeof(p->slug), "%s", slug[0] ? slug : (folder[0] ? folder : name));
        lowercase_ascii(p->slug);
        sanitize_filename(p->slug);
        configure_platform(state, p);
        if (!p->known) {
            cur = end + 1;
            continue;
        }
        p->ids[0] = id;
        p->id_count = 1;

        bool merged = false;
        for (int i = 0; i < state->platform_count; i++) {
            if (strcmp(state->platforms[i].slug, p->slug) == 0) {
                add_platform_id(&state->platforms[i], id);
                merged = true;
                break;
            }
        }
        if (merged) {
            cur = end + 1;
            continue;
        }
        state->platform_count++;
        cur = end + 1;
    }
}

static int extract_collection_count(const char *json) {
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

static bool find_cover_url(const char *start, const char *end, char *dst, size_t dst_size) {
    const char *fields[] = { "path_cover_large", "path_cover_small", "url_cover", "cover_url", "image_url", "thumbnail_url", "thumbnail", "cover", "image", "path_cover_l", "path_cover_s", "path_cover" };
    for (int i = 0; i < 12; i++) {
        if (extract_string(start, end, fields[i], dst, dst_size) && dst[0]) return true;
    }
    return false;
}

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
        snprintf(item->name, sizeof(item->name), "%s", name);
        const char *url_fields[] = { "download_url", "url", "content_url", "path_download", "fs_path", "file_path" };
        extract_first_string(cur, end, url_fields, 6, item->url, sizeof(item->url));
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

static long long parse_rom_size_from_object(const char *start, const char *end) {
    RomFileItem files[32];
    int count = parse_rom_file_objects(start, end, files, 0, 32);
    long long total = sum_file_sizes(files, count);
    if (total > 0) return total;
    return extract_size_bytes(start, end);
}

static long long parse_rom_total_size(const char *json) {
    RomFileItem files[32];
    int count = parse_rom_files(json, files, 32);
    long long total = sum_file_sizes(files, count);
    if (total > 0) return total;
    return extract_size_bytes(json, json + strlen(json));
}

static int parse_rom_files(const char *json, RomFileItem *files, int max_files) {
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

static int parse_roms(AppState *state, int platform_index, int expected_platform_id, const char *json) {
    int before = state->rom_count;
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
    while (state->rom_count < MAX_ROMS && cur < scan_end && (cur = strchr(cur, '{')) != NULL && cur < scan_end) {
        const char *end = find_json_object_end(cur);
        if (!end || end > scan_end) break;
        RomItem *rom = &state->roms[state->rom_count];
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
        rom->size_bytes = parse_rom_size_from_object(cur, end);
        const char *file_fields[] = { "file_name", "fs_name", "filename", "path", "name" };
        if (!extract_first_string(cur, end, file_fields, 5, rom->file_name, sizeof(rom->file_name))) {
            snprintf(rom->file_name, sizeof(rom->file_name), "%.150s.zip", rom->name);
            sanitize_filename(rom->file_name);
        }
        find_cover_url(cur, end, rom->cover_url, sizeof(rom->cover_url));
        snprintf(rom->cover_path, sizeof(rom->cover_path), "%s/%s_%d.png", POSTER_CACHE_DIR, state->platforms[platform_index].slug, rom->id);
        state->rom_count++;
        cur = end + 1;
    }
    return state->rom_count - before;
}

static int count_roms_for_platform(const char *json, int expected_platform_id) {
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

static int count_rom_objects(const char *json) {
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

