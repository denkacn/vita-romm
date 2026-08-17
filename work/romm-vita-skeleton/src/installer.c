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
    bool has_hint = platform_index == 0 && find_vita_zip_hint(archive, &hint);
    PspZipHint psp_hint;
    bool has_psp_hint = platform_index == 1 && find_psp_zip_hint(archive, &psp_hint);
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

        if (platform_index == 0 && vita_title_id_size > 0 && vita_title_id[0] == '\0') {
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

    snprintf(file, sizeof(file), "%s", rom->file_name[0] ? rom->file_name : rom->name);
    sanitize_filename(file);
    url_encode(encoded, sizeof(encoded), file);
    const char *download_dir = state->selected_platform == 0 ? state->vita_download_dir : state->psp_download_dir;
    const char *install_dir = state->selected_platform == 0 ? state->vita_download_dir : state->psp_install_dir;
    if (state->selected_platform == 1 && (ends_with_ci(file, ".iso") || ends_with_ci(file, ".cso"))) {
        snprintf(path, sizeof(path), "%s/%s", install_dir, file);
    } else if (state->selected_platform == 1 && ends_with_ci(file, ".pbp")) {
        char folder[80];
        make_folder_name_from_file(file, folder, sizeof(folder));
        snprintf(path, sizeof(path), "%s/%s/EBOOT.PBP", state->psp_game_dir, folder);
    } else {
        snprintf(path, sizeof(path), "%s/%s", download_dir, file);
    }
    mkdir_parent_for_file(path);
    snprintf(part_path, sizeof(part_path), "%s.part", path);

    char api_path[300];
    snprintf(api_path, sizeof(api_path), "/api/roms/%d/content/%s", rom->id, encoded);
    build_api_url(url, sizeof(url), state, api_path);

    state->busy = true;
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
            int installed = install_zip(state, path, state->selected_platform, &matched, &skipped, vita_title_id, sizeof(vita_title_id), first_failed, sizeof(first_failed));
            if (installed > 0) {
                if (state->selected_platform == 0 && vita_title_id[0]) {
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
            snprintf(state->status, sizeof(state->status), "%s", state->selected_platform == 1 ? "Installed" : "Downloaded");
            snprintf(state->detail, sizeof(state->detail), "%.255s", path);
        }
    } else {
        snprintf(state->status, sizeof(state->status), "Download failed");
        snprintf(state->detail, sizeof(state->detail), "%s / HTTP %ld", curl_easy_strerror(c), http);
    }
    update_rom_status(state, state->selected_rom);
    state->busy = false;
}
