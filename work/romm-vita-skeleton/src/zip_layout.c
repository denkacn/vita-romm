static bool zip_entry_payload(const char *entry, const char *marker, const char **payload) {
    size_t marker_len = strlen(marker);
    if (starts_with(entry, marker)) {
        *payload = entry + marker_len;
        return **payload != '\0';
    }

    char wrapped[40];
    snprintf(wrapped, sizeof(wrapped), "/%s", marker);
    const char *p = strstr(entry, wrapped);
    if (!p) return false;
    *payload = p + strlen(wrapped);
    return **payload != '\0';
}

static bool parse_sfo_title_id(const unsigned char *data, size_t size, char *dst, size_t dst_size) {
    if (size < 20 || data[0] != 0x00 || data[1] != 'P' || data[2] != 'S' || data[3] != 'F') return false;
    unsigned int key_ofs = data[8] | (data[9] << 8) | (data[10] << 16) | (data[11] << 24);
    unsigned int data_ofs = data[12] | (data[13] << 8) | (data[14] << 16) | (data[15] << 24);
    unsigned int entries = data[16] | (data[17] << 8) | (data[18] << 16) | (data[19] << 24);
    if (key_ofs >= size || data_ofs >= size || entries > 128) return false;

    for (unsigned int i = 0; i < entries; i++) {
        size_t item = 20 + i * 16;
        if (item + 16 > size) return false;
        unsigned int key_rel = data[item] | (data[item + 1] << 8);
        unsigned int value_len = data[item + 4] | (data[item + 5] << 8) | (data[item + 6] << 16) | (data[item + 7] << 24);
        unsigned int value_rel = data[item + 12] | (data[item + 13] << 8) | (data[item + 14] << 16) | (data[item + 15] << 24);
        if (key_ofs + key_rel >= size || data_ofs + value_rel >= size) continue;
        const char *key = (const char *)(data + key_ofs + key_rel);
        if (strcmp(key, "TITLE_ID") != 0 && strcmp(key, "TITLEID") != 0) continue;
        size_t value_ofs = data_ofs + value_rel;
        size_t n = value_len;
        if (value_ofs + n > size) n = size - value_ofs;
        if (n >= dst_size) n = dst_size - 1;
        memcpy(dst, data + value_ofs, n);
        dst[n] = '\0';
        trim_line(dst);
        return dst[0] != '\0';
    }
    return false;
}

static int read_zip_entry(zip_t *archive, zip_uint64_t index, unsigned char *buffer, size_t buffer_size) {
    zip_stat_t st;
    zip_stat_init(&st);
    if (zip_stat_index(archive, index, 0, &st) != 0 || st.size > buffer_size) return -1;
    zip_file_t *file = zip_fopen_index(archive, index, 0);
    if (!file) return -1;
    zip_int64_t n = zip_fread(file, buffer, buffer_size);
    zip_fclose(file);
    return n < 0 ? -1 : (int)n;
}

static bool find_vita_zip_hint(zip_t *archive, VitaZipHint *hint) {
    memset(hint, 0, sizeof(*hint));
    zip_int64_t count = zip_get_num_entries(archive, 0);
    if (count < 0) return false;

    unsigned char sfo[32768];
    for (zip_uint64_t i = 0; i < (zip_uint64_t)count; i++) {
        const char *name_raw = zip_get_name(archive, i, 0);
        if (!name_raw) continue;
        char name[320];
        snprintf(name, sizeof(name), "%s", name_raw);
        for (char *p = name; *p; p++) {
            if (*p == '\\') *p = '/';
        }
        if (!ends_with_ci(name, "sce_sys/param.sfo") || path_has_traversal(name)) continue;

        int n = read_zip_entry(archive, i, sfo, sizeof(sfo));
        if (n <= 0 || !parse_sfo_title_id(sfo, (size_t)n, hint->title_id, sizeof(hint->title_id))) continue;

        char *marker = strstr(name, "sce_sys/param.sfo");
        if (!marker) continue;
        size_t root_len = (size_t)(marker - name);
        if (root_len >= sizeof(hint->root)) root_len = sizeof(hint->root) - 1;
        memcpy(hint->root, name, root_len);
        hint->root[root_len] = '\0';
        return true;
    }
    return false;
}

static bool extract_title_id_from_payload(const char *payload, char *dst, size_t dst_size) {
    const char *slash = strchr(payload, '/');
    if (!slash) return false;
    size_t n = (size_t)(slash - payload);
    if (n == 0 || n >= dst_size) return false;
    memcpy(dst, payload, n);
    dst[n] = '\0';
    return true;
}

static void make_folder_name_from_file(const char *file, char *dst, size_t dst_size) {
    snprintf(dst, dst_size, "%s", basename_ptr(file));
    char *dot = strrchr(dst, '.');
    if (dot) *dot = '\0';
    if (dst[0] == '\0' || strcmp(dst, "EBOOT") == 0 || strcmp(dst, "eboot") == 0) {
        snprintf(dst, dst_size, "PBP_GAME");
    }
    sanitize_filename(dst);
}

static void psp_folder_from_root(const char *root, char *dst, size_t dst_size) {
    char temp[180];
    snprintf(temp, sizeof(temp), "%s", root);
    trim_line(temp);
    size_t len = strlen(temp);
    while (len > 0 && temp[len - 1] == '/') temp[--len] = '\0';
    const char *folder = basename_ptr(temp);
    if (!folder || !folder[0]) folder = "PBP_GAME";
    snprintf(dst, dst_size, "%s", folder);
    sanitize_filename(dst);
}

static bool find_psp_zip_hint(zip_t *archive, PspZipHint *hint) {
    memset(hint, 0, sizeof(*hint));
    zip_int64_t count = zip_get_num_entries(archive, 0);
    if (count < 0) return false;

    for (zip_uint64_t i = 0; i < (zip_uint64_t)count; i++) {
        const char *name_raw = zip_get_name(archive, i, 0);
        if (!name_raw) continue;
        char name[320];
        snprintf(name, sizeof(name), "%s", name_raw);
        for (char *p = name; *p; p++) {
            if (*p == '\\') *p = '/';
        }
        if (path_has_traversal(name) || !ends_with_ci(name, "EBOOT.PBP")) continue;

        char *slash = strrchr(name, '/');
        if (slash) {
            size_t root_len = (size_t)(slash - name + 1);
            if (root_len >= sizeof(hint->root)) root_len = sizeof(hint->root) - 1;
            memcpy(hint->root, name, root_len);
            hint->root[root_len] = '\0';
            psp_folder_from_root(hint->root, hint->folder, sizeof(hint->folder));
        } else {
            hint->root[0] = '\0';
            snprintf(hint->folder, sizeof(hint->folder), "PBP_GAME");
        }
        return true;
    }
    return false;
}

static bool get_vita_title_id_for_entry(const char *entry_name, const VitaZipHint *hint, char *dst, size_t dst_size) {
    char entry[320];
    snprintf(entry, sizeof(entry), "%s", entry_name);
    for (char *p = entry; *p; p++) {
        if (*p == '\\') *p = '/';
    }
    if (path_has_traversal(entry)) return false;

    const char *payload = NULL;
    if (zip_entry_payload(entry, "app/", &payload)) {
        return extract_title_id_from_payload(payload, dst, dst_size);
    }
    if (hint && hint->title_id[0] && starts_with(entry, hint->root)) {
        snprintf(dst, dst_size, "%s", hint->title_id);
        return true;
    }
    return false;
}

static bool map_zip_entry_to_install_path(const AppState *state, int platform_index, const char *entry_name, const VitaZipHint *hint, const PspZipHint *psp_hint, char *dst, size_t dst_size) {
    const PlatformDef *platform = &state->platforms[platform_index];
    char entry[320];
    snprintf(entry, sizeof(entry), "%s", entry_name);
    for (char *p = entry; *p; p++) {
        if (*p == '\\') *p = '/';
    }

    size_t len = strlen(entry);
    if (len == 0 || entry[len - 1] == '/' || path_has_traversal(entry)) return false;

    if (platform->install_mode == INSTALL_ADRENALINE_PSP || platform->install_mode == INSTALL_PSX) {
        if (ends_with_ci(entry, ".iso") || ends_with_ci(entry, ".cso")) {
            char file[180];
            snprintf(file, sizeof(file), "%s", basename_ptr(entry));
            sanitize_filename(file);
            snprintf(dst, dst_size, "%s/%s", state->psp_install_dir, file);
            return true;
        }
        if (psp_hint && psp_hint->folder[0]) {
            const char *payload = NULL;
            if (psp_hint->root[0]) {
                if (!starts_with(entry, psp_hint->root)) return false;
                payload = entry + strlen(psp_hint->root);
                if (*payload == '\0') return false;
            } else {
                payload = basename_ptr(entry);
            }
            snprintf(dst, dst_size, "%s/%s/%s", platform->install_mode == INSTALL_PSX ? state->psx_pbp_dir : state->psp_game_dir, psp_hint->folder, payload);
            return true;
        }
        if (ends_with_ci(entry, ".pbp")) {
            char folder[80];
            make_folder_name_from_file(entry, folder, sizeof(folder));
            snprintf(dst, dst_size, "%s/%s/EBOOT.PBP", platform->install_mode == INSTALL_PSX ? state->psx_pbp_dir : state->psp_game_dir, folder);
            return true;
        }
        if (platform->install_mode == INSTALL_PSX && (ends_with_ci(entry, ".cue") || ends_with_ci(entry, ".bin"))) {
            char file[180];
            snprintf(file, sizeof(file), "%s", basename_ptr(entry));
            sanitize_filename(file);
            snprintf(dst, dst_size, "%s/%s", platform->path, file);
            return true;
        }
        return false;
    }

    const char *payload = NULL;
    if (zip_entry_payload(entry, "app/", &payload)) {
        snprintf(dst, dst_size, "%s/%s", state->vita_app_dir, payload);
        return true;
    }
    if (zip_entry_payload(entry, "patch/", &payload)) {
        snprintf(dst, dst_size, "%s/%s", state->vita_patch_dir, payload);
        return true;
    }
    if (zip_entry_payload(entry, "repatch/", &payload)) {
        snprintf(dst, dst_size, "%s/%s", state->vita_repatch_dir, payload);
        return true;
    }
    if (zip_entry_payload(entry, "addcont/", &payload)) {
        snprintf(dst, dst_size, "%s/%s", state->vita_addcont_dir, payload);
        return true;
    }
    if (zip_entry_payload(entry, "license/", &payload)) {
        snprintf(dst, dst_size, "%s/%s", state->vita_license_dir, payload);
        return true;
    }

    if (hint && hint->title_id[0] && starts_with(entry, hint->root)) {
        payload = entry + strlen(hint->root);
        if (*payload == '\0') return false;
        snprintf(dst, dst_size, "%s/%s/%s", state->vita_app_dir, hint->title_id, payload);
        return true;
    }

    return false;
}
