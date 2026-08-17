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

static bool find_json_array_field(const char *json, const char *field, const char **array_start, const char **array_end) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", field);
    const char *key = strstr(json, pat);
    if (!key) return false;
    const char *colon = strchr(key + strlen(pat), ':');
    if (!colon) return false;
    const char *open = strchr(colon, '[');
    if (!open) return false;
    const char *end = find_json_array_end(open);
    if (!end) return false;
    *array_start = open;
    *array_end = end;
    return true;
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
    return false;
}

static void parse_platforms(AppState *state, const char *json) {
    const char *cur = json;
    while ((cur = strchr(cur, '{')) != NULL) {
        const char *end = find_json_object_end(cur);
        if (!end) break;
        int id = -1;
        char name[96] = "";
        char slug[96] = "";
        char folder[96] = "";
        extract_int(cur, end, "id", &id);
        extract_string(cur, end, "name", name, sizeof(name));
        extract_string(cur, end, "slug", slug, sizeof(slug));
        extract_string(cur, end, "fs_slug", folder, sizeof(folder));
        char all[260];
        snprintf(all, sizeof(all), "%s %s %s", name, slug, folder);
        lowercase_ascii(all);
        if (strstr(all, "psvita") || strstr(all, "vita")) state->platforms[0].id = id;
        if (strstr(all, "psp") || strstr(all, "portable")) state->platforms[1].id = id;
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

static int extract_stats_count_for_platform(const AppState *state, int platform_index, const char *json) {
    const PlatformDef *platform = &state->platforms[platform_index];
    const char *cur = json;
    while ((cur = strchr(cur, '{')) != NULL) {
        const char *end = find_json_object_end(cur);
        if (!end) break;

        int count = -1;
        char name[128] = "";
        char slug[96] = "";
        extract_string(cur, end, "platform_name", name, sizeof(name));
        if (!name[0]) extract_string(cur, end, "name", name, sizeof(name));
        extract_string(cur, end, "platform_slug", slug, sizeof(slug));
        if (!slug[0]) extract_string(cur, end, "slug", slug, sizeof(slug));

        char all[260];
        snprintf(all, sizeof(all), "%s %s", name, slug);
        lowercase_ascii(all);

        char label[64];
        snprintf(label, sizeof(label), "%s", platform->label);
        lowercase_ascii(label);

        if ((strstr(all, platform->slug) || strstr(all, label)) &&
            (extract_int(cur, end, "game_count", &count) || extract_int(cur, end, "rom_count", &count) || extract_int(cur, end, "count", &count)) &&
            count >= 0) {
            return count;
        }
        cur = end + 1;
    }
    return -1;
}

static bool find_cover_url(const char *start, const char *end, char *dst, size_t dst_size) {
    const char *fields[] = { "path_cover_large", "path_cover_small", "url_cover", "cover_url", "image_url", "thumbnail_url", "thumbnail", "cover", "image", "path_cover_l", "path_cover_s", "path_cover" };
    for (int i = 0; i < 12; i++) {
        if (extract_string(start, end, fields[i], dst, dst_size) && dst[0]) return true;
    }
    return false;
}

static void parse_roms(AppState *state, int platform_index, const char *json) {
    state->rom_count = 0;
    int platform_id = state->platforms[platform_index].id;
    const char *scan_start = json;
    const char *scan_end = json + strlen(json);
    const char *array_start = NULL;
    const char *array_end = NULL;
    if (find_json_array_field(json, "items", &array_start, &array_end) ||
        find_json_array_field(json, "results", &array_start, &array_end) ||
        find_json_array_field(json, "data", &array_start, &array_end)) {
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
        if (platform_id >= 0 && object_contains(cur, end, "\"platform") && !object_has_platform_id(cur, end, platform_id)) {
            cur = end + 1;
            continue;
        }
        const char *name_fields[] = { "name", "fs_name", "file_name", "filename", "title" };
        if (id < 0 || !extract_first_string(cur, end, name_fields, 5, rom->name, sizeof(rom->name))) {
            cur = end + 1;
            continue;
        }
        rom->id = id;
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
    state->selected_rom = 0;
}

