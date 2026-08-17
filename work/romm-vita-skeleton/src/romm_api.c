static CURLcode get_platform_roms(AppState *state, int platform_id, int limit, long *http_out) {
    char url[380];
    char path[128];

    snprintf(path, sizeof(path), "/api/roms?platform_ids=%d&limit=%d&offset=0", platform_id, limit);
    build_api_url(url, sizeof(url), state, path);
    CURLcode c = http_get(state, url, http_response, http_out);
    if (c == CURLE_OK && *http_out == 200) return c;

    snprintf(path, sizeof(path), "/api/roms?platform_id=%d&limit=%d&offset=0", platform_id, limit);
    build_api_url(url, sizeof(url), state, path);
    return http_get(state, url, http_response, http_out);
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

static bool load_cover_url_from_rom_detail(AppState *state, RomItem *rom) {
    char api_path[64];
    char url[360];
    long http = 0;

    snprintf(api_path, sizeof(api_path), "/api/roms/%d", rom->id);
    build_api_url(url, sizeof(url), state, api_path);
    CURLcode c = http_get(state, url, http_response, &http);
    if (c != CURLE_OK || http != 200) {
        char log_line[220];
        snprintf(log_line, sizeof(log_line), "Cover detail failed: rom=%d http=%ld curl=%d", rom->id, http, c);
        append_install_log(log_line);
        return false;
    }

    if (find_cover_url(http_response, http_response + strlen(http_response), rom->cover_url, sizeof(rom->cover_url))) {
        char log_line[320];
        snprintf(log_line, sizeof(log_line), "Cover detail found: rom=%d url=%.230s", rom->id, rom->cover_url);
        append_install_log(log_line);
        return true;
    }

    char log_line[180];
    snprintf(log_line, sizeof(log_line), "Cover detail missing: rom=%d name=%.100s", rom->id, rom->name);
    append_install_log(log_line);
    return false;
}

static void load_current_cover(AppState *state) {
    if (state->rom_count <= 0) return;
    update_rom_status(state, state->selected_rom);
    RomItem *rom = &state->roms[state->selected_rom];
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

        if (!rom->cover_url[0]) {
            load_cover_url_from_rom_detail(state, rom);
        }

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

    build_api_url(url, sizeof(url), state, "/api/stats?include_platform_stats=true");
    c = http_get(state, url, stats_response, &http);
    bool stats_ok = c == CURLE_OK && http == 200;

    for (int p = 0; p < PLATFORM_COUNT; p++) {
        state->platforms[p].rom_count = 0;
        if (state->platforms[p].id < 0) continue;
        snprintf(state->status, sizeof(state->status), "Loading %s", state->platforms[p].label);

        if (stats_ok) {
            int stats_count = extract_stats_count_for_platform(state, p, stats_response);
            if (stats_count >= 0) {
                state->platforms[p].rom_count = stats_count;
                continue;
            }
        }

        c = get_platform_roms(state, state->platforms[p].id, 1, &http);
        if (c == CURLE_OK && http == 200) {
            state->platforms[p].rom_count = extract_collection_count(http_response);
        }
    }

    snprintf(state->status, sizeof(state->status), "Ready");
    snprintf(state->detail, sizeof(state->detail), "Choose platform");
    state->view = VIEW_PLATFORMS;
    state->busy = false;
}

static void load_games(AppState *state, int platform_index) {
    PlatformDef *p = &state->platforms[platform_index];
    if (p->id < 0) {
        snprintf(state->status, sizeof(state->status), "%s not found", p->label);
        return;
    }
    state->busy = true;
    snprintf(state->status, sizeof(state->status), "Loading %s games", p->label);
    long http = 0;
    CURLcode c = get_platform_roms(state, p->id, MAX_ROMS, &http);
    if (c != CURLE_OK || http != 200) {
        snprintf(state->status, sizeof(state->status), "ROM list error");
        snprintf(state->detail, sizeof(state->detail), "%s / HTTP %ld", curl_easy_strerror(c), http);
        state->busy = false;
        return;
    }
    parse_roms(state, platform_index, http_response);
    snprintf(state->status, sizeof(state->status), "%s games: %d", p->label, state->rom_count);
    snprintf(state->detail, sizeof(state->detail), "RomM platform id: %d. Left/Right browse, X download, O back", p->id);
    state->view = VIEW_GAMES;
    state->busy = false;
    load_current_cover(state);
}
