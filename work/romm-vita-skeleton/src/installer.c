static int copy_zip_file_to_path(zip_t *archive, zip_uint64_t index, const char *dst) {
    zip_file_t *src = zip_fopen_index(archive, index, 0);
    if (!src) return -100;

    mkdir_parent_for_file(dst);
    SceUID out = sceIoOpen(dst, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, VITA_FILE_MODE);
    if (out < 0) {
        zip_fclose(src);
        return out;
    }

    char buffer[16384];
    int result = 0;
    while (1) {
        zip_int64_t n = zip_fread(src, buffer, sizeof(buffer));
        if (n < 0) {
            result = -200;
            break;
        }
        if (n == 0) break;
        int written = sceIoWrite(out, buffer, (SceSize)n);
        if (written != n) {
            result = written < 0 ? written : -201;
            break;
        }
    }

    sceIoClose(out);
    zip_fclose(src);
    if (result < 0) sceIoRemove(dst);
    return result;
}

static int install_zip(AppState *state, const char *zip_path, int platform_index, int *matched, int *skipped, char *vita_title_id, size_t vita_title_id_size, char *first_failed, size_t first_failed_size) {
    int err = 0;
    zip_t *archive = zip_open(zip_path, ZIP_RDONLY, &err);
    if (!archive) return -1;

    int installed = 0;
    *matched = 0;
    *skipped = 0;
    if (first_failed_size > 0) first_failed[0] = '\0';
    VitaZipHint hint;
    bool has_hint = state->platforms[platform_index].install_mode == INSTALL_VITA && find_vita_zip_hint(archive, &hint);
    PspZipHint psp_hint;
    bool has_psp_hint = state->platforms[platform_index].install_mode == INSTALL_ADRENALINE_PSP && find_psp_zip_hint(archive, &psp_hint);
    if (vita_title_id_size > 0) vita_title_id[0] = '\0';
    zip_int64_t count = zip_get_num_entries(archive, 0);
    if (count < 0) {
        zip_close(archive);
        return -1;
    }
    for (zip_uint64_t i = 0; i < (zip_uint64_t)count; i++) {
        if ((i % 8) == 0 || i + 1 == (zip_uint64_t)count) {
            snprintf(state->status, sizeof(state->status), "Installing ZIP");
            snprintf(state->detail, sizeof(state->detail), "Entry %llu / %llu", (unsigned long long)(i + 1), (unsigned long long)count);
            redraw_now(state);
        }

        const char *name = zip_get_name(archive, i, 0);
        if (!name) {
            (*skipped)++;
            continue;
        }

        if (state->platforms[platform_index].install_mode == INSTALL_VITA && vita_title_id_size > 0 && vita_title_id[0] == '\0') {
            get_vita_title_id_for_entry(name, has_hint ? &hint : NULL, vita_title_id, vita_title_id_size);
        }

        char dst[384];
        if (!map_zip_entry_to_install_path(state, platform_index, name, has_hint ? &hint : NULL, has_psp_hint ? &psp_hint : NULL, dst, sizeof(dst))) {
            (*skipped)++;
            continue;
        }
        (*matched)++;

        int copy_res = copy_zip_file_to_path(archive, i, dst);
        if (copy_res == 0) {
            installed++;
        } else {
            (*skipped)++;
            if (first_failed_size > 0 && first_failed[0] == '\0') {
                snprintf(first_failed, first_failed_size, "0x%08X %.110s -> %.110s", (unsigned int)copy_res, name, dst);
            }
        }
    }

    zip_close(archive);
    if (first_failed_size > 0 && first_failed[0] != '\0') {
        char log_line[520];
        snprintf(log_line, sizeof(log_line), "ZIP copy failed: %s", first_failed);
        append_install_log(log_line);
    }
    return installed;
}

static int install_folder_zip(AppState *state, const char *zip_path, int platform_index, const RomItem *rom, int *matched, int *skipped, char *first_failed, size_t first_failed_size) {
    int err = 0;
    zip_t *archive = zip_open(zip_path, ZIP_RDONLY, &err);
    if (!archive) return -1;

    PlatformDef *p = &state->platforms[platform_index];
    bool multi_psx = p->install_mode == INSTALL_PSX;
    char game_folder[180];
    rom_folder_name(rom, game_folder, sizeof(game_folder));

    *matched = 0;
    *skipped = 0;
    if (first_failed_size > 0) first_failed[0] = '\0';
    int installed = 0;
    zip_int64_t count = zip_get_num_entries(archive, 0);
    if (count < 0) {
        zip_close(archive);
        return -1;
    }

    for (zip_uint64_t i = 0; i < (zip_uint64_t)count; i++) {
        const char *raw = zip_get_name(archive, i, 0);
        if (!raw) {
            (*skipped)++;
            continue;
        }
        char entry[320];
        snprintf(entry, sizeof(entry), "%s", raw);
        for (char *c = entry; *c; c++) if (*c == '\\') *c = '/';
        size_t len = strlen(entry);
        if (len == 0 || entry[len - 1] == '/' || path_has_traversal(entry) || !extension_matches_platform(entry, p)) {
            (*skipped)++;
            continue;
        }

        char dst[384];
        char file[180];
        snprintf(file, sizeof(file), "%s", basename_ptr(entry));
        sanitize_filename(file);
        if (multi_psx && ends_with_ci(file, ".pbp")) {
            snprintf(dst, sizeof(dst), "%s/%s/EBOOT.PBP", state->psx_pbp_dir, game_folder);
        } else if (multi_psx && (ends_with_ci(file, ".cue") || ends_with_ci(file, ".bin"))) {
            snprintf(dst, sizeof(dst), "%s/%s/%s", p->path, game_folder, file);
        } else {
            snprintf(dst, sizeof(dst), "%s/%s", p->path, file);
        }

        (*matched)++;
        if (matched && *matched > 1 && !multi_psx) continue;
        snprintf(state->status, sizeof(state->status), "Installing ZIP");
        snprintf(state->detail, sizeof(state->detail), "%.160s", file);
        redraw_now(state);
        int copy_res = copy_zip_file_to_path(archive, i, dst);
        if (copy_res == 0) {
            installed++;
        } else {
            (*skipped)++;
            if (first_failed_size > 0 && first_failed[0] == '\0') {
                snprintf(first_failed, first_failed_size, "0x%08X %.110s", (unsigned int)copy_res, file);
            }
        }
    }
    zip_close(archive);
    return installed;
}

static void build_rom_content_url(AppState *state, const RomItem *rom, const char *file_name, char *url, size_t url_size) {
    char file[320];
    char encoded[420];
    char api_path[560];
    snprintf(file, sizeof(file), "%s", file_name && file_name[0] ? file_name : (rom->file_name[0] ? rom->file_name : rom->name));
    url_encode(encoded, sizeof(encoded), file);
    snprintf(api_path, sizeof(api_path), "/api/roms/%d/content/%s", rom->id, encoded);
    build_api_url(url, url_size, state, api_path);
}

static bool download_rom_file_to(AppState *state, const RomItem *rom, const char *source_name, const char *maybe_url, const char *dst) {
    if (usable_file_exists(dst)) {
        snprintf(state->status, sizeof(state->status), "Using installed file");
        snprintf(state->detail, sizeof(state->detail), "%.230s", dst);
        redraw_now(state);
        return true;
    }

    char url[640];
    char part_path[384];
    long http = 0;
    if (maybe_url && maybe_url[0] && (starts_with(maybe_url, "http://") || starts_with(maybe_url, "https://") || starts_with(maybe_url, "/"))) {
        make_absolute_url(url, sizeof(url), state, maybe_url);
    } else {
        build_rom_content_url(state, rom, source_name, url, sizeof(url));
    }
    mkdir_parent_for_file(dst);
    snprintf(part_path, sizeof(part_path), "%s.part", dst);
    remove_path_recursive(part_path);

    snprintf(state->status, sizeof(state->status), "Downloading");
    snprintf(state->detail, sizeof(state->detail), "%.180s", source_name);
    redraw_now(state);
    CURLcode c = http_download(state, url, part_path, &http);
    if (c == CURLE_OK && http == 200) {
        remove_path_recursive(dst);
        if (sceIoRename(part_path, dst) >= 0) return true;
    }
    remove_path_recursive(part_path);
    snprintf(state->status, sizeof(state->status), "Download failed");
    snprintf(state->detail, sizeof(state->detail), "%s / HTTP %ld", curl_easy_strerror(c), http);
    return false;
}

static int install_psx_multi_files(AppState *state, RomItem *rom, PlatformDef *p) {
    RomFileItem files[24];
    int count = load_rom_file_list(state, rom, files, 24);
    bool has_cue = false;
    bool has_bin = false;
    int target_count = 0;
    long long target_size = 0;
    for (int i = 0; i < count; i++) {
        if (ends_with_ci(files[i].name, ".cue")) {
            has_cue = true;
            target_count++;
            if (files[i].size_bytes > 0) target_size += files[i].size_bytes;
        } else if (ends_with_ci(files[i].name, ".bin")) {
            has_bin = true;
            target_count++;
            if (files[i].size_bytes > 0) target_size += files[i].size_bytes;
        }
    }
    if (!has_cue || !has_bin) return 0;
    if (target_size > 0) rom->size_bytes = target_size;

    char folder[180];
    rom_folder_name(rom, folder, sizeof(folder));
    int ok = 0;
    int total = 0;
    char log_line[420];
    snprintf(log_line, sizeof(log_line), "PSX multi-file start: rom=%d files=%d targets=%d folder=%s", rom->id, count, target_count, folder);
    append_install_log(log_line);
    for (int i = 0; i < count; i++) {
        if (!ends_with_ci(files[i].name, ".cue") && !ends_with_ci(files[i].name, ".bin")) continue;
        char file[180];
        snprintf(file, sizeof(file), "%s", basename_ptr(files[i].name));
        sanitize_filename(file);
        char dst[384];
        snprintf(dst, sizeof(dst), "%s/%s/%s", p->path, folder, file);
        total++;
        state->progress_current = total;
        state->progress_total = target_count;
        snprintf(state->status, sizeof(state->status), "Downloading file %d/%d", total, target_count);
        snprintf(state->detail, sizeof(state->detail), "%.180s", file);
        redraw_now(state);
        snprintf(log_line, sizeof(log_line), "PSX file %d/%d: source=%.180s dst=%.180s", total, target_count, files[i].name, dst);
        append_install_log(log_line);
        if (download_rom_file_to(state, rom, files[i].name, files[i].url, dst)) ok++;
    }
    state->progress_current = 0;
    state->progress_total = 0;
    if (ok == total && total > 0) {
        snprintf(state->status, sizeof(state->status), "Installed");
        snprintf(state->detail, sizeof(state->detail), "PSX files: %d -> %s/%s", ok, p->path, folder);
        return 1;
    }
    snprintf(state->status, sizeof(state->status), "Install incomplete");
    snprintf(state->detail, sizeof(state->detail), "PSX files: %d / %d", ok, total);
    snprintf(log_line, sizeof(log_line), "PSX multi-file incomplete: rom=%d ok=%d total=%d no fallback", rom->id, ok, total);
    append_install_log(log_line);
    return 1;
}

static void download_selected_rom(AppState *state) {
    if (state->rom_count <= 0) return;
    PlatformDef *p = &state->platforms[state->selected_platform];
    RomItem *rom = &state->roms[state->selected_rom];
    char encoded[240];
    char url[460];
    char path[340];
    char part_path[360];
    char file[180];
    long http = 0;

    if (p->support_level == SUPPORT_PATH_REQUIRED) {
        snprintf(state->status, sizeof(state->status), "Path required");
        snprintf(state->detail, sizeof(state->detail), "Add platform.%s.path to %s", p->slug, CONFIG_PATH);
        return;
    }

    state->busy = true;
    if (p->install_mode == INSTALL_PSX) {
        int psx_multi = install_psx_multi_files(state, rom, p);
        if (psx_multi != 0) {
            update_rom_status(state, state->selected_rom);
            state->busy = false;
            return;
        }
    }

    snprintf(file, sizeof(file), "%s", rom->file_name[0] ? rom->file_name : rom->name);
    sanitize_filename(file);
    url_encode(encoded, sizeof(encoded), file);
    char custom_cache[180];
    const char *download_dir = state->vita_download_dir;
    const char *install_dir = state->vita_download_dir;
    if (p->install_mode == INSTALL_ADRENALINE_PSP) {
        download_dir = state->psp_download_dir;
        install_dir = state->psp_install_dir;
    } else if (p->install_mode == INSTALL_FOLDER || p->install_mode == INSTALL_PSX) {
        platform_cache_dir(p, custom_cache, sizeof(custom_cache));
        download_dir = custom_cache;
        install_dir = p->path;
    }
    if (p->install_mode == INSTALL_ADRENALINE_PSP && (ends_with_ci(file, ".iso") || ends_with_ci(file, ".cso"))) {
        snprintf(path, sizeof(path), "%s/%s", install_dir, file);
    } else if ((p->install_mode == INSTALL_ADRENALINE_PSP || p->install_mode == INSTALL_PSX) && ends_with_ci(file, ".pbp")) {
        char folder[80];
        make_folder_name_from_file(file, folder, sizeof(folder));
        snprintf(path, sizeof(path), "%s/%s/EBOOT.PBP", p->install_mode == INSTALL_PSX ? state->psx_pbp_dir : state->psp_game_dir, folder);
    } else if (p->install_mode == INSTALL_FOLDER || p->install_mode == INSTALL_PSX) {
        snprintf(path, sizeof(path), "%s/%s", ends_with_ci(file, ".zip") ? download_dir : install_dir, file);
    } else {
        snprintf(path, sizeof(path), "%s/%s", download_dir, file);
    }
    mkdir_parent_for_file(path);
    snprintf(part_path, sizeof(part_path), "%s.part", path);

    char api_path[300];
    snprintf(api_path, sizeof(api_path), "/api/roms/%d/content/%s", rom->id, encoded);
    build_api_url(url, sizeof(url), state, api_path);

    CURLcode c = CURLE_OK;
    if (usable_file_exists(path)) {
        http = 200;
        snprintf(state->status, sizeof(state->status), "Using cached file");
        snprintf(state->detail, sizeof(state->detail), "%.255s", path);
        redraw_now(state);
    } else {
        remove_path_recursive(part_path);
        snprintf(state->status, sizeof(state->status), "Downloading");
        snprintf(state->detail, sizeof(state->detail), "%s -> %s", rom->name, p->slug);
        redraw_now(state);
        c = http_download(state, url, part_path, &http);
        if (c == CURLE_OK && http == 200) {
            remove_path_recursive(path);
            int rename_res = sceIoRename(part_path, path);
            if (rename_res < 0) {
                c = CURLE_WRITE_ERROR;
            }
        }
    }
    if (c == CURLE_OK && http == 200) {
        if (ends_with_ci(path, ".zip")) {
            snprintf(state->status, sizeof(state->status), "Installing ZIP");
            snprintf(state->detail, sizeof(state->detail), "%.255s", path);
            redraw_now(state);
            int skipped = 0;
            int matched = 0;
            char vita_title_id[16];
            char first_failed[256];
            int installed = 0;
            if (p->install_mode == INSTALL_FOLDER || p->install_mode == INSTALL_PSX) {
                installed = install_folder_zip(state, path, state->selected_platform, rom, &matched, &skipped, first_failed, sizeof(first_failed));
            } else {
                installed = install_zip(state, path, state->selected_platform, &matched, &skipped, vita_title_id, sizeof(vita_title_id), first_failed, sizeof(first_failed));
            }
            if (installed > 0) {
                if (p->install_mode == INSTALL_VITA && vita_title_id[0]) {
                    char missing[224];
                    bool app_ok = validate_vita_install(state, vita_title_id, missing, sizeof(missing));
                    if (!app_ok) {
                        snprintf(state->status, sizeof(state->status), "Installed, app invalid");
                        if (first_failed[0]) {
                            snprintf(state->detail, sizeof(state->detail), "Files:%d/%d missing:%.90s fail:%.70s", installed, matched, missing, first_failed);
                        } else {
                            snprintf(state->detail, sizeof(state->detail), "Files:%d matched:%d missing: %.150s", installed, matched, missing);
                        }
                        char log_line[520];
                        snprintf(log_line, sizeof(log_line), "Install invalid: title=%s files=%d matched=%d skipped=%d missing=%s first_failed=%s", vita_title_id, installed, matched, skipped, missing, first_failed);
                        append_install_log(log_line);
                    } else
                    if (state->auto_refresh_livearea) {
                        snprintf(state->status, sizeof(state->status), "Refreshing LiveArea");
                        snprintf(state->detail, sizeof(state->detail), "%s", vita_title_id);
                        redraw_now(state);
                        int promote = refresh_vita_livearea_for_title(state, vita_title_id);
                        if (promote >= 0) {
                            snprintf(state->status, sizeof(state->status), "Installed");
                            snprintf(state->detail, sizeof(state->detail), "Files: %d, skipped: %d. LiveArea refreshed: %s", installed, skipped, vita_title_id);
                        } else {
                            snprintf(state->status, sizeof(state->status), "Installed, refresh failed");
                            snprintf(state->detail, sizeof(state->detail), "Files: %d, skipped: %d. Try VitaShell refresh. Promoter: 0x%08X", installed, skipped, (unsigned int)promote);
                        }
                    } else {
                        snprintf(state->status, sizeof(state->status), "Installed");
                        snprintf(state->detail, sizeof(state->detail), "App OK: %s. Press Triangle or use VitaShell Refresh LiveArea", vita_title_id);
                    }
                } else {
                    snprintf(state->status, sizeof(state->status), "Installed");
                    snprintf(state->detail, sizeof(state->detail), "Files: %d, skipped: %d", installed, skipped);
                }
            } else if (installed == 0) {
                if (matched > 0) {
                    snprintf(state->status, sizeof(state->status), "ZIP extract failed");
                    snprintf(state->detail, sizeof(state->detail), "Matched:%d skipped:%d fail:%.130s", matched, skipped, first_failed[0] ? first_failed : "Check ZIP compression/encryption");
                } else {
                    snprintf(state->status, sizeof(state->status), "Downloaded, not installed");
                    snprintf(state->detail, sizeof(state->detail), "Unknown ZIP layout. Saved: %.190s", path);
                }
            } else {
                snprintf(state->status, sizeof(state->status), "ZIP open failed");
                snprintf(state->detail, sizeof(state->detail), "Saved: %.220s", path);
            }
        } else {
            snprintf(state->status, sizeof(state->status), "%s", p->install_mode == INSTALL_VITA ? "Downloaded" : "Installed");
            snprintf(state->detail, sizeof(state->detail), "%.255s", path);
        }
    } else {
        snprintf(state->status, sizeof(state->status), "Download failed");
        snprintf(state->detail, sizeof(state->detail), "%s / HTTP %ld", curl_easy_strerror(c), http);
    }
    update_rom_status(state, state->selected_rom);
    state->busy = false;
}
