static CURLcode get_platform_roms_page(AppState *state, int platform_id, const char *slug, int limit, int offset, long *http_out) {
    char url[380];
    char path[160];
    const char *formats[] = {
        "/api/roms?platform_ids=%d&limit=%d&offset=%d",
        "/api/roms?platform_id=%d&limit=%d&offset=%d",
        "/api/roms?platform=%d&limit=%d&offset=%d"
    };
    CURLcode last_c = CURLE_OK;
    long last_http = 0;

    for (int i = 0; i < 3; i++) {
        snprintf(path, sizeof(path), formats[i], platform_id, limit, offset);
        build_api_url(url, sizeof(url), state, path);
        last_c = http_get(state, url, http_response, http_out);
        last_http = *http_out;
        if (last_c != CURLE_OK || *http_out != 200) continue;

        int objects = count_rom_objects(http_response);
        int matched = count_roms_for_platform(http_response, platform_id);
        int total = extract_collection_count(http_response);
        if (offset == 0) {
            char log_line[360];
            snprintf(log_line, sizeof(log_line), "Platform probe slug=%s id=%d query=%d http=%ld total=%d objects=%d matched=%d", slug, platform_id, i, *http_out, total, objects, matched);
            append_install_log(log_line);
        }
        if (matched > 0 || objects == 0 || total == 0) return last_c;
    }

    *http_out = last_http;
    return last_c;
}

static int get_platform_rom_count(AppState *state, const PlatformDef *platform, long *http_out) {
    int total = 0;
    int ids = platform->id_count > 0 ? platform->id_count : 1;
    for (int i = 0; i < ids; i++) {
        int platform_id = platform->id_count > 0 ? platform->ids[i] : platform->id;
        int offset = 0;
        while (offset < MAX_ROMS) {
            CURLcode c = get_platform_roms_page(state, platform_id, platform->slug, 50, offset, http_out);
            if (c != CURLE_OK || *http_out != 200) break;
            int count = count_roms_for_platform(http_response, platform_id);
            if (count <= 0) break;
            total += count;
            if (count < 50) break;
            offset += 50;
        }
    }
    char log_line[320];
    snprintf(log_line, sizeof(log_line), "Platform count slug=%s label=%s id=%d ids=%d rom_count=%d", platform->slug, platform->label, platform->id, platform->id_count, total);
    append_install_log(log_line);
    return total;
}

static void make_absolute_url(char *dst, size_t dst_size, const AppState *state, const char *maybe_path) {
    if (starts_with(maybe_path, "http://") || starts_with(maybe_path, "https://")) {
        snprintf(dst, dst_size, "%s", maybe_path);
    } else if (starts_with(maybe_path, "/")) {
        build_api_url(dst, dst_size, state, maybe_path);
    } else {
        char path[340];
        snprintf(path, sizeof(path), "/%s", maybe_path);
        build_api_url(dst, dst_size, state, path);
    }
    escape_url_spaces(dst, dst_size);
}

static bool load_rom_metadata_from_detail(AppState *state, RomItem *rom) {
    char api_path[64];
    char url[360];
    long http = 0;

    snprintf(api_path, sizeof(api_path), "/api/roms/%d", rom->id);
    build_api_url(url, sizeof(url), state, api_path);
    CURLcode c = http_get(state, url, http_response, &http);
    if (c != CURLE_OK || http != 200) {
        char log_line[220];
        snprintf(log_line, sizeof(log_line), "ROM detail failed: rom=%d http=%ld curl=%d", rom->id, http, c);
        append_install_log(log_line);
        return false;
    }

    long long size = parse_rom_total_size(http_response);
    if (size > 0) rom->size_bytes = size;

    if (find_cover_url(http_response, http_response + strlen(http_response), rom->cover_url, sizeof(rom->cover_url))) {
        char log_line[320];
        snprintf(log_line, sizeof(log_line), "ROM detail found: rom=%d size=%lld url=%.210s", rom->id, rom->size_bytes, rom->cover_url);
        append_install_log(log_line);
        return true;
    }

    char log_line[180];
    snprintf(log_line, sizeof(log_line), "ROM detail cover missing: rom=%d size=%lld name=%.90s", rom->id, rom->size_bytes, rom->name);
    append_install_log(log_line);
    return size > 0;
}

static int load_rom_file_list(AppState *state, const RomItem *rom, RomFileItem *files, int max_files) {
    char api_path[64];
    char url[360];
    long http = 0;

    snprintf(api_path, sizeof(api_path), "/api/roms/%d", rom->id);
    build_api_url(url, sizeof(url), state, api_path);
    CURLcode c = http_get(state, url, http_response, &http);
    if (c != CURLE_OK || http != 200) {
        char log_line[220];
        snprintf(log_line, sizeof(log_line), "Rom files detail failed: rom=%d http=%ld curl=%d", rom->id, http, c);
        append_install_log(log_line);
        return 0;
    }
    return parse_rom_files(http_response, files, max_files);
}

static void load_cached_platform_icon(AppState *state, PlatformDef *p) {
    if (p->known || p->icon || !p->slug[0]) return;
    const char *exts[] = { "png", "jpg", "jpeg" };
    char path[160];
    char url_path[120];
    char url[360];
    long http = 0;

    for (int i = 0; i < 3; i++) {
        snprintf(path, sizeof(path), "%s/%s.%s", PLATFORM_ICON_CACHE_DIR, p->slug, exts[i]);
        if (!usable_file_exists(path)) {
            snprintf(url_path, sizeof(url_path), "/assets/platforms/%s.%s", p->slug, exts[i]);
            build_api_url(url, sizeof(url), state, url_path);
            snprintf(state->status, sizeof(state->status), "Loading platform icon");
            snprintf(state->detail, sizeof(state->detail), "%s", p->label);
            redraw_now(state);
            CURLcode c = http_download(state, url, path, &http);
            if (c != CURLE_OK || http != 200) {
                sceIoRemove(path);
                continue;
            }
        }
        p->icon = vita2d_load_PNG_file(path);
        if (!p->icon) p->icon = vita2d_load_JPEG_file(path);
        if (p->icon) {
            snprintf(p->icon_path, sizeof(p->icon_path), "%s", path);
            return;
        }
        sceIoRemove(path);
    }
}

static void load_current_cover(AppState *state) {
    if (state->rom_count <= 0) return;
    update_rom_status(state, state->selected_rom);
    RomItem *rom = &state->roms[state->selected_rom];
    if (!rom->cover_url[0] || rom->size_bytes <= 0) {
        load_rom_metadata_from_detail(state, rom);
    }
    if (rom->cover) return;

    SceIoStat st;
    bool need_download = sceIoGetstat(rom->cover_path, &st) < 0 || st.st_size <= 0;
    if (!need_download) {
        rom->cover = vita2d_load_PNG_file(rom->cover_path);
        if (!rom->cover) rom->cover = vita2d_load_JPEG_file(rom->cover_path);
        if (rom->cover) return;
        sceIoRemove(rom->cover_path);
        need_download = true;
    }

    if (need_download) {
        char url[400];
        char api_path[96];
        const char *fallbacks[] = {
            "/api/roms/%d/assets/cover",
            "/api/roms/%d/cover",
            "/api/assets/roms/%d/cover",
            "/api/roms/%d/assets/cover_l"
        };
        int attempts = 0;
        bool downloaded = false;

        if (rom->cover_url[0]) {
            long status = 0;
            make_absolute_url(url, sizeof(url), state, rom->cover_url);
            CURLcode c = http_download(state, url, rom->cover_path, &status);
            attempts++;
            if (c == CURLE_OK && status == 200) {
                downloaded = true;
            } else {
                sceIoRemove(rom->cover_path);
                char log_line[520];
                snprintf(log_line, sizeof(log_line), "Cover failed: rom=%d try=%d http=%ld curl=%d url=%.320s", rom->id, attempts, status, c, url);
                append_install_log(log_line);
            }
        } else {
            char log_line[220];
            snprintf(log_line, sizeof(log_line), "Cover missing in RomM API: rom=%d name=%.120s", rom->id, rom->name);
            append_install_log(log_line);
        }

        for (int i = 0; rom->cover_url[0] && !downloaded && i < 4; i++) {
            long status = 0;
            snprintf(api_path, sizeof(api_path), fallbacks[i], rom->id);
            build_api_url(url, sizeof(url), state, api_path);
            CURLcode c = http_download(state, url, rom->cover_path, &status);
            attempts++;
            if (c == CURLE_OK && status == 200) {
                downloaded = true;
            } else {
                sceIoRemove(rom->cover_path);
                char log_line[520];
                snprintf(log_line, sizeof(log_line), "Cover failed: rom=%d try=%d http=%ld curl=%d url=%.320s", rom->id, attempts, status, c, url);
                append_install_log(log_line);
            }
        }
    }

    rom->cover = vita2d_load_PNG_file(rom->cover_path);
    if (!rom->cover) rom->cover = vita2d_load_JPEG_file(rom->cover_path);
    if (!rom->cover) {
        char log_line[220];
        snprintf(log_line, sizeof(log_line), "Cover decode failed: rom=%d path=%s source=%.80s", rom->id, rom->cover_path, rom->cover_url);
        append_install_log(log_line);
        sceIoRemove(rom->cover_path);
    }
}

static void load_platforms_and_roms(AppState *state) {
    state->busy = true;
    snprintf(state->status, sizeof(state->status), "Loading platforms");
    snprintf(state->detail, sizeof(state->detail), "GET /api/platforms");

    char url[360];
    long http = 0;
    build_api_url(url, sizeof(url), state, "/api/platforms");
    CURLcode c = http_get(state, url, http_response, &http);
    if (c != CURLE_OK || http != 200) {
        snprintf(state->status, sizeof(state->status), "RomM API error");
        snprintf(state->detail, sizeof(state->detail), "platforms: %s / %ld", curl_easy_strerror(c), http);
        state->busy = false;
        return;
    }
    parse_platforms(state, http_response);
    if (state->platform_count <= 0) {
        snprintf(state->status, sizeof(state->status), "No platforms");
        snprintf(state->detail, sizeof(state->detail), "RomM returned no platforms");
        state->busy = false;
        return;
    }

    for (int p = 0; p < state->platform_count; p++) {
        state->platforms[p].rom_count = 0;
        if (state->platforms[p].id < 0) continue;
        snprintf(state->status, sizeof(state->status), "Loading %s", state->platforms[p].label);

        state->platforms[p].rom_count = get_platform_rom_count(state, &state->platforms[p], &http);
    }

    int out = 0;
    for (int i = 0; i < state->platform_count; i++) {
        if (state->platforms[i].rom_count <= 0) continue;
        if (out != i) state->platforms[out] = state->platforms[i];
        out++;
    }
    state->platform_count = out;
    if (state->selected_platform >= state->platform_count) state->selected_platform = 0;
    if (state->platform_count <= 0) {
        snprintf(state->status, sizeof(state->status), "No games");
        snprintf(state->detail, sizeof(state->detail), "No RomM platforms have games");
        state->busy = false;
        return;
    }

    for (int i = 0; i < state->platform_count; i++) {
        load_cached_platform_icon(state, &state->platforms[i]);
    }

    snprintf(state->status, sizeof(state->status), "Ready");
    snprintf(state->detail, sizeof(state->detail), "Choose platform");
    state->view = VIEW_PLATFORMS;
    state->busy = false;
}

static void load_games(AppState *state, int platform_index) {
    if (platform_index < 0 || platform_index >= state->platform_count) return;
    PlatformDef *p = &state->platforms[platform_index];
    if (p->id < 0) {
        snprintf(state->status, sizeof(state->status), "%s not found", p->label);
        return;
    }
    state->busy = true;
    snprintf(state->status, sizeof(state->status), "Loading %s games", p->label);
    state->rom_count = 0;
    state->selected_rom = 0;
    long http = 0;
    CURLcode c = CURLE_OK;
    int page_size = 50;
    int ids = p->id_count > 0 ? p->id_count : 1;
    for (int id_index = 0; id_index < ids && state->rom_count < MAX_ROMS; id_index++) {
        int platform_id = p->id_count > 0 ? p->ids[id_index] : p->id;
        int offset = 0;
        while (state->rom_count < MAX_ROMS) {
            snprintf(state->detail, sizeof(state->detail), "RomM id %d offset %d", platform_id, offset);
            redraw_now(state);
            c = get_platform_roms_page(state, platform_id, p->slug, page_size, offset, &http);
            if (c != CURLE_OK || http != 200) break;
            int added = parse_roms(state, platform_index, platform_id, http_response);
            int total = extract_collection_count(http_response);
            if (added <= 0) break;
            offset += page_size;
            if (total > 0 && offset >= total) break;
            if (added < page_size) break;
        }
        if (c != CURLE_OK || http != 200) break;
    }
    if ((c != CURLE_OK || http != 200) && state->rom_count <= 0) {
        snprintf(state->status, sizeof(state->status), "ROM list error");
        snprintf(state->detail, sizeof(state->detail), "%s / HTTP %ld", curl_easy_strerror(c), http);
        state->busy = false;
        return;
    }
    snprintf(state->status, sizeof(state->status), "%s games: %d", p->label, state->rom_count);
    if (p->support_level == SUPPORT_PATH_REQUIRED) {
        snprintf(state->detail, sizeof(state->detail), "Add platform.%s.path to config.txt", p->slug);
    } else if (p->support_level == SUPPORT_UNTESTED) {
        snprintf(state->detail, sizeof(state->detail), "Untested on PS Vita. Downloads to: %.120s", p->path);
    } else if (p->install_mode == INSTALL_FOLDER || p->install_mode == INSTALL_PSX) {
        snprintf(state->detail, sizeof(state->detail), "Downloads to: %.150s", p->path);
    } else {
        snprintf(state->detail, sizeof(state->detail), "RomM platform id: %d. Left/Right browse, X download, O back", p->id);
    }
    state->view = VIEW_GAMES;
    state->busy = false;
    load_current_cover(state);
}
