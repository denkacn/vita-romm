static CURLcode get_platform_roms_page_into(AppState *state, int platform_id, const char *slug, int limit, int offset, char *response, long *http_out) {
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
        last_c = http_get(state, url, response, http_out);
        last_http = *http_out;
        if (last_c != CURLE_OK || *http_out != 200) continue;

        int objects = count_rom_objects(response);
        int matched = count_roms_for_platform(response, platform_id);
        int total = extract_collection_count(response);
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

static CURLcode get_platform_roms_page(AppState *state, int platform_id, const char *slug, int limit, int offset, long *http_out) {
    return get_platform_roms_page_into(state, platform_id, slug, limit, offset, http_response, http_out);
}

static CURLcode get_platform_roms_page_fast(AppState *state, CURL *curl, int platform_id, int limit, int offset, char *response, long *http_out) {
    char url[380];
    char path[160];
    snprintf(path, sizeof(path), "/api/roms?platform_ids=%d&limit=%d&offset=%d&order_by=name&order_dir=asc&with_extra=false&with_rom_id_index=false", platform_id, limit, offset);
    build_api_url(url, sizeof(url), state, path);
    return http_get_reuse(state, curl, url, response, http_out);
}

static unsigned long long now_ms(void) {
    return (unsigned long long)(sceKernelGetProcessTimeWide() / 1000ULL);
}

static int get_platform_rom_count(AppState *state, const PlatformDef *platform, long *http_out) {
    int total = 0;
    int ids = platform->id_count > 0 ? platform->id_count : 1;
    for (int i = 0; i < ids; i++) {
        int platform_id = platform->id_count > 0 ? platform->ids[i] : platform->id;
        CURLcode c = get_platform_roms_page(state, platform_id, platform->slug, 50, 0, http_out);
        if (c != CURLE_OK || *http_out != 200) continue;
        int api_total = extract_collection_count(http_response);
        int page_count = count_roms_for_platform(http_response, platform_id);
        if (api_total > 0) total += api_total;
        else total += page_count;
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

static void load_current_cover_from_cache(AppState *state) {
    if (state->rom_count <= 0) return;
    RomItem *rom = &state->roms[state->selected_rom];
    if (rom->cover) return;

    SceIoStat st;
    if (sceIoGetstat(rom->cover_path, &st) < 0 || st.st_size <= 0) return;
    rom->cover = vita2d_load_PNG_file(rom->cover_path);
    if (!rom->cover) rom->cover = vita2d_load_JPEG_file(rom->cover_path);
    if (!rom->cover) sceIoRemove(rom->cover_path);
}

static void lock_games(AppState *state);
static void unlock_games(AppState *state);

typedef struct {
    AppState *state;
    int generation;
    int rom_index;
    int rom_id;
    char cover_url[320];
    char cover_path[128];
} CoverWorkerArgs;

static int cover_worker_entry(SceSize args_size, void *argp) {
    (void)args_size;
    CoverWorkerArgs *args = NULL;
    if (argp) memcpy(&args, argp, sizeof(args));
    if (!args || !args->state) return 0;

    AppState *state = args->state;
    char cover_url[320];
    char cover_path[128];
    snprintf(cover_url, sizeof(cover_url), "%s", args->cover_url);
    snprintf(cover_path, sizeof(cover_path), "%s", args->cover_path);

    char *response = NULL;
    if (!cover_url[0]) {
        response = (char *)malloc(HTTP_RESPONSE_MAX + 1);
        if (response) {
            char api_path[64];
            char url[360];
            long http = 0;
            snprintf(api_path, sizeof(api_path), "/api/roms/%d", args->rom_id);
            build_api_url(url, sizeof(url), state, api_path);
            CURLcode c = http_get(state, url, response, &http);
            if (c == CURLE_OK && http == 200) {
                find_cover_url(response, response + strlen(response), cover_url, sizeof(cover_url));
                lock_games(state);
                if (args->generation == state->cover_worker_generation &&
                    args->rom_index >= 0 && args->rom_index < state->rom_count &&
                    state->roms[args->rom_index].id == args->rom_id &&
                    cover_url[0]) {
                    snprintf(state->roms[args->rom_index].cover_url, sizeof(state->roms[args->rom_index].cover_url), "%s", cover_url);
                }
                unlock_games(state);
            }
        }
    }

    SceIoStat st;
    if (cover_url[0] && sceIoGetstat(cover_path, &st) < 0) {
        char url[420];
        char part_path[160];
        long http = 0;
        make_absolute_url(url, sizeof(url), state, cover_url);
        snprintf(part_path, sizeof(part_path), "%s.part", cover_path);
        remove_path_recursive(part_path);
        mkdir_parent_for_file(cover_path);
        CURLcode c = http_download_quiet(state, url, part_path, &http);
        if (c == CURLE_OK && http == 200) {
            remove_path_recursive(cover_path);
            if (sceIoRename(part_path, cover_path) < 0) remove_path_recursive(part_path);
        } else {
            remove_path_recursive(part_path);
            char log_line[420];
            snprintf(log_line, sizeof(log_line), "Cover background failed: rom=%d http=%ld curl=%d url=%.240s", args->rom_id, http, c, url);
            append_install_log(log_line);
        }
    }

    lock_games(state);
    if (args->generation == state->cover_worker_generation) state->cover_worker_running = false;
    unlock_games(state);
    if (response) free(response);
    free(args);
    return 0;
}

static void start_current_cover_worker(AppState *state) {
    if (state->rom_count <= 0 || state->selected_rom < 0 || state->selected_rom >= state->rom_count) return;
    RomItem *rom = &state->roms[state->selected_rom];
    if (rom->cover || state->cover_worker_running) return;

    SceIoStat st;
    if (sceIoGetstat(rom->cover_path, &st) >= 0 && st.st_size > 0) return;

    CoverWorkerArgs *args = (CoverWorkerArgs *)malloc(sizeof(CoverWorkerArgs));
    if (!args) return;
    memset(args, 0, sizeof(*args));
    state->cover_worker_generation++;
    state->cover_worker_running = true;
    args->state = state;
    args->generation = state->cover_worker_generation;
    args->rom_index = state->selected_rom;
    args->rom_id = rom->id;
    snprintf(args->cover_url, sizeof(args->cover_url), "%s", rom->cover_url);
    snprintf(args->cover_path, sizeof(args->cover_path), "%s", rom->cover_path);

    SceUID thid = sceKernelCreateThread("vita_romm_cover", cover_worker_entry, 0x10000100, 128 * 1024, 0, 0, NULL);
    if (thid < 0) {
        state->cover_worker_running = false;
        free(args);
        return;
    }
    state->cover_worker_thread = thid;
    CoverWorkerArgs *thread_args = args;
    int start = sceKernelStartThread(thid, sizeof(thread_args), &thread_args);
    if (start < 0) {
        state->cover_worker_generation++;
        state->cover_worker_running = false;
        free(args);
    }
}

static void ensure_current_cover_loading(AppState *state) {
    if (state->rom_count <= 0 || state->selected_rom < 0 || state->selected_rom >= state->rom_count) return;
    RomItem *rom = &state->roms[state->selected_rom];
    if (!rom->cover) load_current_cover_from_cache(state);
    if (!rom->cover) start_current_cover_worker(state);
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

static void set_platform_game_detail(AppState *state, const PlatformDef *p) {
    if (p->support_level == SUPPORT_PATH_REQUIRED) {
        snprintf(state->detail, sizeof(state->detail), "Add platform.%s.path to config.txt", p->slug);
    } else if (p->support_level == SUPPORT_UNTESTED) {
        snprintf(state->detail, sizeof(state->detail), "Untested on PS Vita. Downloads to: %.120s", p->path);
    } else if (p->install_mode == INSTALL_FOLDER || p->install_mode == INSTALL_PSX) {
        snprintf(state->detail, sizeof(state->detail), "Downloads to: %.150s", p->path);
    } else {
        snprintf(state->detail, sizeof(state->detail), "RomM platform id: %d. Left/Right browse, X download, O back", p->id);
    }
}

typedef struct {
    AppState *state;
    PlatformDef platform;
    int platform_index;
    int generation;
} GameWorkerArgs;

static void lock_games(AppState *state) {
    if (state->games_lock >= 0) sceKernelWaitSema(state->games_lock, 1, NULL);
}

static void unlock_games(AppState *state) {
    if (state->games_lock >= 0) sceKernelSignalSema(state->games_lock, 1);
}

static bool worker_generation_alive(AppState *state, int generation) {
    return generation == state->games_worker_generation;
}

static void append_worker_roms(AppState *state, int generation, const PlatformDef *p, const RomItem *items, int count, int total_hint) {
    lock_games(state);
    if (!worker_generation_alive(state, generation)) {
        unlock_games(state);
        return;
    }
    for (int i = 0; i < count && state->rom_count < MAX_ROMS; i++) {
        state->roms[state->rom_count++] = items[i];
    }
    if (total_hint > 0) state->platforms[state->selected_platform].rom_count = total_hint;
    snprintf(state->status, sizeof(state->status), "%s games: %d / %d", p->label, state->rom_count, total_hint > 0 ? total_hint : state->rom_count);
    set_platform_game_detail(state, p);
    unlock_games(state);
}

static int games_worker_entry(SceSize args_size, void *argp) {
    (void)args_size;
    GameWorkerArgs *args = NULL;
    if (argp) memcpy(&args, argp, sizeof(args));
    if (!args || !args->state) return 0;

    AppState *state = args->state;
    PlatformDef p = args->platform;
    int platform_index = args->platform_index;
    int generation = args->generation;
    int page_size = 50;
    char *response = (char *)malloc(HTTP_RESPONSE_MAX + 1);
    if (!response) {
        lock_games(state);
        if (worker_generation_alive(state, generation)) {
            snprintf(state->status, sizeof(state->status), "ROM list error");
            snprintf(state->detail, sizeof(state->detail), "Out of memory");
            state->games_worker_running = false;
            state->games_loading_more = false;
        }
        unlock_games(state);
        free(args);
        return 0;
    }
    AppState *page_state = (AppState *)malloc(sizeof(AppState));
    if (!page_state) {
        lock_games(state);
        if (worker_generation_alive(state, generation)) {
            snprintf(state->status, sizeof(state->status), "ROM list error");
            snprintf(state->detail, sizeof(state->detail), "Out of memory");
            state->games_worker_running = false;
            state->games_loading_more = false;
        }
        unlock_games(state);
        free(response);
        free(args);
        return 0;
    }
    CURL *page_curl = curl_easy_init();

    int ids = p.id_count > 0 ? p.id_count : 1;
    for (int id_index = 0; id_index < ids && worker_generation_alive(state, generation); id_index++) {
        int platform_id = p.id_count > 0 ? p.ids[id_index] : p.id;
        int offset = 0;
        while (worker_generation_alive(state, generation)) {
            long http = 0;
            unsigned long long t0 = now_ms();
            CURLcode c = get_platform_roms_page_fast(state, page_curl, platform_id, page_size, offset, response, &http);
            unsigned long long t1 = now_ms();
            if (c != CURLE_OK || http != 200) {
                char log_line[240];
                snprintf(log_line, sizeof(log_line), "Game worker failed slug=%s id=%d offset=%d http=%ld curl=%d http_ms=%llu", p.slug, platform_id, offset, http, c, t1 - t0);
                append_install_log(log_line);
                break;
            }

            memset(page_state, 0, sizeof(*page_state));
            page_state->platform_count = state->platform_count;
            page_state->platforms[platform_index] = p;
            int added = parse_roms(page_state, platform_index, -1, response);
            unsigned long long t2 = now_ms();
            int total = extract_collection_count(response);
            append_worker_roms(state, generation, &p, page_state->roms, page_state->rom_count, total);
            unsigned long long t3 = now_ms();

            char log_line[260];
            snprintf(log_line, sizeof(log_line), "Game worker page slug=%s id=%d offset=%d added=%d total=%d bytes=%u http_ms=%llu parse_ms=%llu append_ms=%llu", p.slug, platform_id, offset, added, total, (unsigned int)strlen(response), t1 - t0, t2 - t1, t3 - t2);
            append_install_log(log_line);

            if (added <= 0 || added < page_size || (total > 0 && offset + page_size >= total)) break;
            offset += page_size;
        }
    }

    if (page_curl) curl_easy_cleanup(page_curl);

    lock_games(state);
    if (worker_generation_alive(state, generation)) {
        state->games_worker_running = false;
        state->games_loading_more = false;
        snprintf(state->status, sizeof(state->status), "%s games: %d", p.label, state->rom_count);
        set_platform_game_detail(state, &p);
    }
    unlock_games(state);

    free(page_state);
    free(response);
    free(args);
    return 0;
}

static void start_games_worker(AppState *state, int platform_index, const PlatformDef *p) {
    state->games_worker_generation++;
    state->games_worker_running = true;
    state->games_loading_more = true;
    state->games_loading_platform = platform_index;
    state->games_loading_id_index = 0;
    state->games_loading_offset = 0;
    state->games_loading_page_size = 50;

    GameWorkerArgs *args = (GameWorkerArgs *)malloc(sizeof(GameWorkerArgs));
    if (!args) {
        state->games_worker_running = false;
        state->games_loading_more = false;
        snprintf(state->status, sizeof(state->status), "ROM list error");
        snprintf(state->detail, sizeof(state->detail), "Out of memory");
        return;
    }
    memset(args, 0, sizeof(*args));
    args->state = state;
    args->platform = *p;
    args->platform_index = platform_index;
    args->generation = state->games_worker_generation;

    SceUID thid = sceKernelCreateThread("vita_romm_games", games_worker_entry, 0x10000100, 256 * 1024, 0, 0, NULL);
    if (thid < 0) {
        free(args);
        state->games_worker_running = false;
        state->games_loading_more = false;
        snprintf(state->status, sizeof(state->status), "ROM list error");
        snprintf(state->detail, sizeof(state->detail), "Worker create failed 0x%08X", (unsigned int)thid);
        return;
    }
    state->games_worker_thread = thid;
    GameWorkerArgs *thread_args = args;
    int start = sceKernelStartThread(thid, sizeof(thread_args), &thread_args);
    if (start < 0) {
        state->games_worker_generation++;
        state->games_worker_running = false;
        state->games_loading_more = false;
        snprintf(state->status, sizeof(state->status), "ROM list error");
        snprintf(state->detail, sizeof(state->detail), "Worker start failed 0x%08X", (unsigned int)start);
        free(args);
    }
}

static void load_games(AppState *state, int platform_index) {
    if (platform_index < 0 || platform_index >= state->platform_count) return;
    PlatformDef *p = &state->platforms[platform_index];
    if (p->id < 0) {
        snprintf(state->status, sizeof(state->status), "%s not found", p->label);
        return;
    }
    state->games_worker_generation++;
    state->busy = false;
    snprintf(state->status, sizeof(state->status), "Loading %s games", p->label);
    state->rom_count = 0;
    state->selected_rom = 0;
    state->view = VIEW_GAMES;
    set_platform_game_detail(state, p);
    snprintf(state->detail, sizeof(state->detail), "Loading first 50...");
    start_games_worker(state, platform_index, p);
}
