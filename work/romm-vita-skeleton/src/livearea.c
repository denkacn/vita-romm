static int load_sce_paf(void) {
    SceSysmoduleOpt opt;
    memset(&opt, 0, sizeof(opt));
    opt.result = &opt.flags;
    uint32_t args[] = { 0x400000, 0xEA60, 0x40000, 0, 0 };
    return sceSysmoduleLoadModuleInternalWithArg(SCE_SYSMODULE_INTERNAL_PAF, sizeof(args), args, &opt);
}

static int unload_sce_paf(void) {
    return sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_PAF);
}

static int promote_app_from_temp(const char *path) {
    int res = load_sce_paf();
    if (res < 0) {
        char log_line[160];
        snprintf(log_line, sizeof(log_line), "Promoter PAF load failed: 0x%08X path=%s", (unsigned int)res, path);
        append_install_log(log_line);
        return res;
    }

    res = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    if (res >= 0) {
        res = scePromoterUtilityInit();
        if (res >= 0) {
            res = scePromoterUtilityPromotePkgWithRif(path, 1);
            if (res < 0) {
                char log_line[180];
                snprintf(log_line, sizeof(log_line), "PromotePkgWithRif failed: 0x%08X path=%s", (unsigned int)res, path);
                append_install_log(log_line);
                res = scePromoterUtilityPromotePkg(path, 1);
                if (res < 0) {
                    snprintf(log_line, sizeof(log_line), "PromotePkg failed: 0x%08X path=%s", (unsigned int)res, path);
                    append_install_log(log_line);
                }
            }
            scePromoterUtilityExit();
        } else {
            char log_line[180];
            snprintf(log_line, sizeof(log_line), "Promoter init failed: 0x%08X path=%s", (unsigned int)res, path);
            append_install_log(log_line);
        }
        sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    } else {
        char log_line[180];
        snprintf(log_line, sizeof(log_line), "Promoter module load failed: 0x%08X path=%s", (unsigned int)res, path);
        append_install_log(log_line);
    }
    unload_sce_paf();
    return res;
}

static int copy_first_license_from_dir(const char *license_dir, const char *work_bin_path) {
    SceUID dfd = sceIoDopen(license_dir);
    if (dfd < 0) return dfd;

    int result = -1;
    while (1) {
        SceIoDirent ent;
        memset(&ent, 0, sizeof(ent));
        int res = sceIoDread(dfd, &ent);
        if (res <= 0) break;
        if (SCE_S_ISDIR(ent.d_stat.st_mode)) continue;
        if (!ends_with_ci(ent.d_name, ".rif") && !ends_with_ci(ent.d_name, ".bin")) continue;

        char rif_path[256];
        snprintf(rif_path, sizeof(rif_path), "%s/%s", license_dir, ent.d_name);
        result = copy_file(rif_path, work_bin_path);
        if (result >= 0) break;
    }

    sceIoDclose(dfd);
    return result;
}

static int copy_first_license_to_work_bin(const char *title_id, const char *work_bin_path) {
    char license_dir[192];
    snprintf(license_dir, sizeof(license_dir), "ux0:license/app/%s", title_id);
    int res = copy_first_license_from_dir(license_dir, work_bin_path);
    if (res >= 0) return res;

    snprintf(license_dir, sizeof(license_dir), "ux0:nonpdrm/license/app/%s", title_id);
    return copy_first_license_from_dir(license_dir, work_bin_path);
}

static int prepare_promoter_package(const char *app_path, const char *title_id) {
    char check_path[256];
    SceIoStat st;
    memset(&st, 0, sizeof(st));

    snprintf(check_path, sizeof(check_path), "%s/eboot.bin", app_path);
    if (sceIoGetstat(check_path, &st) < 0) return -0x101;

    snprintf(check_path, sizeof(check_path), "%s/sce_sys/param.sfo", app_path);
    if (sceIoGetstat(check_path, &st) < 0) return -0x102;

    snprintf(check_path, sizeof(check_path), "%s/sce_sys/package", app_path);
    sceIoMkdir(check_path, VITA_DIR_MODE);

    char work_bin_path[256];
    snprintf(work_bin_path, sizeof(work_bin_path), "%s/sce_sys/package/work.bin", app_path);
    if (sceIoGetstat(work_bin_path, &st) >= 0) return 0;

    int res = copy_first_license_to_work_bin(title_id, work_bin_path);
    if (res >= 0) return 0;

    char log_line[260];
    snprintf(log_line, sizeof(log_line), "No work.bin/rif for title=%s app_path=%s last=0x%08X", title_id, app_path, (unsigned int)res);
    append_install_log(log_line);
    return -0x103;
}

static int refresh_vita_livearea_for_path(const char *src, const char *temp, const char *title_id, bool needs_work_bin) {
    if (!title_id || !title_id[0]) return -1;

    sceIoMkdir("ux0:temp", VITA_DIR_MODE);
    remove_path_recursive(temp);

    int res = sceIoRename(src, temp);
    if (res < 0) return res;

    if (needs_work_bin) {
        res = prepare_promoter_package(temp, title_id);
        if (res < 0) {
            sceIoRename(temp, src);
            return res;
        }
    }

    res = promote_app_from_temp(temp);
    if (res < 0) {
        sceIoRename(temp, src);
        return res;
    }

    remove_path_recursive(temp);
    return 0;
}

static int refresh_vita_livearea_for_title(const AppState *state, const char *title_id) {
    char src[192];
    snprintf(src, sizeof(src), "%s/%s", state->vita_app_dir, title_id);
    return refresh_vita_livearea_for_path(src, "ux0:temp/app", title_id, true);
}

static int refresh_vita_patch_for_title(const AppState *state, const char *title_id) {
    char src[192];
    snprintf(src, sizeof(src), "%s/%s", state->vita_patch_dir, title_id);
    return refresh_vita_livearea_for_path(src, "ux0:temp/patch", title_id, false);
}

static bool extension_matches_platform(const char *name, const PlatformDef *p) {
    if (p->install_mode == INSTALL_ADRENALINE_PSP) {
        return ends_with_ci(name, ".iso") || ends_with_ci(name, ".cso") || ends_with_ci(name, ".pbp") || ends_with_ci(name, ".zip");
    }
    if (p->install_mode == INSTALL_PSX) {
        return ends_with_ci(name, ".pbp") || ends_with_ci(name, ".cue") || ends_with_ci(name, ".bin") || ends_with_ci(name, ".zip");
    }
    if (strcmp(p->slug, "gba") == 0) return ends_with_ci(name, ".gba") || ends_with_ci(name, ".zip");
    if (strcmp(p->slug, "gb") == 0) return ends_with_ci(name, ".gb") || ends_with_ci(name, ".zip");
    if (strcmp(p->slug, "gbc") == 0) return ends_with_ci(name, ".gbc") || ends_with_ci(name, ".zip");
    if (strcmp(p->slug, "nes") == 0) return ends_with_ci(name, ".nes") || ends_with_ci(name, ".zip");
    if (strcmp(p->slug, "snes") == 0) return ends_with_ci(name, ".sfc") || ends_with_ci(name, ".smc") || ends_with_ci(name, ".zip");
    if (strcmp(p->slug, "genesis") == 0) return ends_with_ci(name, ".md") || ends_with_ci(name, ".gen") || ends_with_ci(name, ".bin") || ends_with_ci(name, ".zip");
    if (strcmp(p->slug, "n64") == 0) return ends_with_ci(name, ".z64") || ends_with_ci(name, ".n64") || ends_with_ci(name, ".v64") || ends_with_ci(name, ".zip");
    return true;
}

static bool psx_file_is_supported(const char *name) {
    return ends_with_ci(name, ".cue") || ends_with_ci(name, ".bin") ||
        ends_with_ci(name, ".pbp") || ends_with_ci(name, ".chd") ||
        ends_with_ci(name, ".iso") || ends_with_ci(name, ".img") ||
        ends_with_ci(name, ".ccd") || ends_with_ci(name, ".sub") ||
        ends_with_ci(name, ".m3u") || ends_with_ci(name, ".zip");
}

static bool psx_file_is_direct_install(const char *name) {
    return psx_file_is_supported(name) && !ends_with_ci(name, ".zip");
}

static void platform_cache_dir(const PlatformDef *p, char *path, size_t path_size) {
    snprintf(path, path_size, "%s/%s", DOWNLOAD_DIR, p->slug[0] ? p->slug : "custom");
}

static void rom_folder_name(const RomItem *rom, char *folder, size_t folder_size) {
    snprintf(folder, folder_size, "%s", rom->name[0] ? rom->name : rom->file_name);
    char *dot = strrchr(folder, '.');
    if (dot) *dot = '\0';
    sanitize_filename(folder);
    if (!folder[0]) snprintf(folder, folder_size, "rom_%d", rom->id);
}

static void rom_download_path(const AppState *state, const RomItem *rom, int platform_index, char *path, size_t path_size) {
    const PlatformDef *p = &state->platforms[platform_index];
    char file[180];
    snprintf(file, sizeof(file), "%s", rom->file_name[0] ? rom->file_name : rom->name);
    sanitize_filename(file);
    if (p->install_mode == INSTALL_ADRENALINE_PSP && (ends_with_ci(file, ".iso") || ends_with_ci(file, ".cso"))) {
        snprintf(path, path_size, "%s/%s", state->psp_install_dir, file);
    } else if (p->install_mode == INSTALL_ADRENALINE_PSP && ends_with_ci(file, ".pbp")) {
        char folder[80];
        make_folder_name_from_file(file, folder, sizeof(folder));
        snprintf(path, path_size, "%s/%s/EBOOT.PBP", state->psp_game_dir, folder);
    } else if (p->install_mode == INSTALL_PSX && ends_with_ci(file, ".pbp")) {
        char folder[80];
        make_folder_name_from_file(file, folder, sizeof(folder));
        snprintf(path, path_size, "%s/%s/EBOOT.PBP", state->psx_pbp_dir, folder);
    } else if (p->install_mode == INSTALL_PSX && (ends_with_ci(file, ".cue") || ends_with_ci(file, ".bin"))) {
        snprintf(path, path_size, "%s/%s", p->path, file);
    } else if (p->install_mode == INSTALL_FOLDER) {
        snprintf(path, path_size, "%s/%s", p->path, file);
    } else if (p->install_mode == INSTALL_PSX) {
        char custom_cache[180];
        platform_cache_dir(p, custom_cache, sizeof(custom_cache));
        snprintf(path, path_size, "%s/%s", ends_with_ci(file, ".zip") ? custom_cache : p->path, file);
    } else {
        char custom_cache[180];
        const char *download_dir = state->vita_download_dir;
        if (p->install_mode == INSTALL_ADRENALINE_PSP) download_dir = state->psp_download_dir;
        else if (p->install_mode != INSTALL_VITA) {
            platform_cache_dir(p, custom_cache, sizeof(custom_cache));
            download_dir = custom_cache;
        }
        snprintf(path, path_size, "%s/%s", download_dir, file);
    }
}

static bool dir_has_file_with_ext(const char *path, const char *ext) {
    SceUID dfd = sceIoDopen(path);
    if (dfd < 0) return false;
    bool found = false;
    while (1) {
        SceIoDirent ent;
        memset(&ent, 0, sizeof(ent));
        int res = sceIoDread(dfd, &ent);
        if (res <= 0) break;
        if (SCE_S_ISDIR(ent.d_stat.st_mode)) continue;
        if (ends_with_ci(ent.d_name, ext)) {
            found = true;
            break;
        }
    }
    sceIoDclose(dfd);
    return found;
}

static bool psx_folder_has_playable_content(const AppState *state, const RomItem *rom, int platform_index) {
    const PlatformDef *p = &state->platforms[platform_index];
    if (p->install_mode != INSTALL_PSX || !p->path_configured || !p->path[0]) return false;

    char folder[180];
    char path[340];
    rom_folder_name(rom, folder, sizeof(folder));
    snprintf(path, sizeof(path), "%s/%s", p->path, folder);
    return dir_exists(path) &&
        (dir_has_file_with_ext(path, ".cue") ||
         dir_has_file_with_ext(path, ".pbp") ||
         dir_has_file_with_ext(path, ".chd") ||
         dir_has_file_with_ext(path, ".iso") ||
         dir_has_file_with_ext(path, ".img") ||
         dir_has_file_with_ext(path, ".ccd") ||
         dir_has_file_with_ext(path, ".m3u"));
}

static void psx_rom_file_destination(const AppState *state, const RomItem *rom, const RomFileItem *file_item, int platform_index, char *path, size_t path_size) {
    const PlatformDef *p = &state->platforms[platform_index];
    char file[180];
    snprintf(file, sizeof(file), "%s", basename_ptr(file_item->name));
    sanitize_filename(file);

    if (ends_with_ci(file, ".pbp")) {
        char folder[80];
        make_folder_name_from_file(file, folder, sizeof(folder));
        snprintf(path, path_size, "%s/%s/EBOOT.PBP", state->psx_pbp_dir, folder);
    } else if (ends_with_ci(file, ".zip")) {
        char custom_cache[180];
        platform_cache_dir(p, custom_cache, sizeof(custom_cache));
        snprintf(path, path_size, "%s/%s", custom_cache, file);
    } else {
        char folder[180];
        rom_folder_name(rom, folder, sizeof(folder));
        snprintf(path, path_size, "%s/%s/%s", p->path, folder, file);
    }
}

static bool psx_expected_files_status(AppState *state, RomItem *rom, int platform_index, bool installed_mode) {
    RomFileItem files[32];
    int count = load_rom_file_list(state, rom, files, 32);
    int expected = 0;
    int present = 0;
    bool has_archive = false;

    for (int i = 0; i < count; i++) {
        if (!psx_file_is_supported(files[i].name)) continue;
        if (ends_with_ci(files[i].name, ".zip")) has_archive = true;
        if (installed_mode && !psx_file_is_direct_install(files[i].name)) continue;

        char path[384];
        psx_rom_file_destination(state, rom, &files[i], platform_index, path, sizeof(path));
        expected++;
        if (usable_file_exists(path)) present++;
    }

    if (expected > 0) {
        char log_line[300];
        snprintf(log_line, sizeof(log_line), "PSX status rom=%d mode=%s present=%d expected=%d archives=%d", rom->id, installed_mode ? "installed" : "downloaded", present, expected, has_archive ? 1 : 0);
        append_install_log(log_line);
        return present == expected;
    }

    if (installed_mode && has_archive) return psx_folder_has_playable_content(state, rom, platform_index);
    return false;
}

static bool extract_title_id_from_text(const char *text, char *dst, size_t dst_size) {
    size_t len = strlen(text);
    for (size_t i = 0; i + 9 <= len; i++) {
        char c0 = text[i];
        if (c0 != 'P') continue;
        bool ok = true;
        for (size_t j = 0; j < 9; j++) {
            char c = text[i + j];
            bool digit = c >= '0' && c <= '9';
            bool upper = c >= 'A' && c <= 'Z';
            if (!digit && !upper) {
                ok = false;
                break;
            }
        }
        if (ok) {
            size_t n = dst_size > 10 ? 9 : dst_size - 1;
            memcpy(dst, text + i, n);
            dst[n] = '\0';
            return true;
        }
    }
    return false;
}

static bool vita_title_id_for_rom(const AppState *state, const RomItem *rom, int platform_index, char *dst, size_t dst_size) {
    dst[0] = '\0';
    if (extract_title_id_from_text(rom->file_name, dst, dst_size)) return true;
    if (extract_title_id_from_text(rom->name, dst, dst_size)) return true;

    char path[340];
    rom_download_path(state, rom, platform_index, path, sizeof(path));
    if (!usable_file_exists(path) || !ends_with_ci(path, ".zip")) return false;

    int err = 0;
    zip_t *archive = zip_open(path, ZIP_RDONLY, &err);
    if (!archive) return false;
    VitaZipHint hint;
    bool found = find_vita_zip_hint(archive, &hint);
    zip_close(archive);
    if (!found || !hint.title_id[0]) return false;
    snprintf(dst, dst_size, "%s", hint.title_id);
    return true;
}

static bool rom_is_downloaded(AppState *state, RomItem *rom, int platform_index) {
    if (state->platforms[platform_index].install_mode == INSTALL_PSX) {
        return psx_expected_files_status(state, rom, platform_index, false) ||
            psx_folder_has_playable_content(state, rom, platform_index);
    }
    char path[340];
    rom_download_path(state, rom, platform_index, path, sizeof(path));
    return usable_file_exists(path);
}

static bool rom_is_installed(AppState *state, RomItem *rom, int platform_index) {
    const PlatformDef *p = &state->platforms[platform_index];
    if (p->install_mode == INSTALL_VITA) {
        char title_id[16];
        if (!vita_title_id_for_rom(state, rom, platform_index, title_id, sizeof(title_id))) return false;
        char path[224];
        snprintf(path, sizeof(path), "%s/%s/eboot.bin", state->vita_app_dir, title_id);
        return usable_file_exists(path);
    }
    if (p->support_level == SUPPORT_PATH_REQUIRED || !p->path_configured || !p->path[0]) return false;
    if (p->install_mode == INSTALL_PSX) {
        return psx_expected_files_status(state, rom, platform_index, true) ||
            psx_folder_has_playable_content(state, rom, platform_index);
    }

    char file[180];
    snprintf(file, sizeof(file), "%s", rom->file_name[0] ? rom->file_name : rom->name);
    sanitize_filename(file);
    if (p->install_mode == INSTALL_ADRENALINE_PSP && (ends_with_ci(file, ".iso") || ends_with_ci(file, ".cso"))) {
        char path[340];
        snprintf(path, sizeof(path), "%s/%s", state->psp_install_dir, file);
        return usable_file_exists(path);
    }
    if ((p->install_mode == INSTALL_ADRENALINE_PSP || p->install_mode == INSTALL_PSX) && ends_with_ci(file, ".pbp")) {
        char path[340];
        char folder[80];
        make_folder_name_from_file(file, folder, sizeof(folder));
        snprintf(path, sizeof(path), "%s/%s/EBOOT.PBP", p->install_mode == INSTALL_PSX ? state->psx_pbp_dir : state->psp_game_dir, folder);
        return usable_file_exists(path);
    }
    if (p->install_mode == INSTALL_PSX && psx_file_is_direct_install(file)) {
        char path[340];
        snprintf(path, sizeof(path), "%s/%s", p->path, file);
        if (usable_file_exists(path)) return true;

        return psx_folder_has_playable_content(state, rom, platform_index);
    }
    if (p->install_mode == INSTALL_FOLDER) {
        char path[340];
        snprintf(path, sizeof(path), "%s/%s", p->path, file);
        return usable_file_exists(path);
    }
    if (!ends_with_ci(file, ".zip")) return false;

    char zip_path[340];
    rom_download_path(state, rom, platform_index, zip_path, sizeof(zip_path));
    if (!usable_file_exists(zip_path)) return false;

    int err = 0;
    zip_t *archive = zip_open(zip_path, ZIP_RDONLY, &err);
    if (!archive) return false;
    PspZipHint hint;
    bool has_hint = (p->install_mode == INSTALL_ADRENALINE_PSP || p->install_mode == INSTALL_PSX) && find_psp_zip_hint(archive, &hint);
    if (has_hint && hint.folder[0]) {
        zip_close(archive);
        char path[340];
        snprintf(path, sizeof(path), "%s/%s/EBOOT.PBP", p->install_mode == INSTALL_PSX ? state->psx_pbp_dir : state->psp_game_dir, hint.folder);
        return usable_file_exists(path);
    }
    zip_int64_t count = zip_get_num_entries(archive, 0);
    bool installed = false;
    bool psx_cue = false;
    bool psx_bin = false;
    for (zip_uint64_t i = 0; count > 0 && i < (zip_uint64_t)count; i++) {
        const char *name = zip_get_name(archive, i, 0);
        if (!name) continue;
        if (p->install_mode == INSTALL_ADRENALINE_PSP && !ends_with_ci(name, ".iso") && !ends_with_ci(name, ".cso")) continue;
        if ((p->install_mode == INSTALL_FOLDER || p->install_mode == INSTALL_PSX) && !extension_matches_platform(name, p)) continue;
        if (p->install_mode == INSTALL_PSX && ends_with_ci(name, ".cue")) psx_cue = true;
        if (p->install_mode == INSTALL_PSX && ends_with_ci(name, ".bin")) psx_bin = true;
        char file[180];
        snprintf(file, sizeof(file), "%s", basename_ptr(name));
        sanitize_filename(file);
        char path[340];
        snprintf(path, sizeof(path), "%s/%s", p->install_mode == INSTALL_ADRENALINE_PSP ? state->psp_install_dir : p->path, file);
        if (usable_file_exists(path)) {
            installed = true;
            break;
        }
    }
    zip_close(archive);
    if (p->install_mode == INSTALL_PSX && psx_cue && psx_bin) {
        return psx_folder_has_playable_content(state, rom, platform_index);
    }
    if (installed) return true;
    return false;
}

static void update_rom_status(AppState *state, int index) {
    if (index < 0 || index >= state->rom_count) return;
    RomItem *rom = &state->roms[index];
    rom->is_downloaded = rom_is_downloaded(state, rom, state->selected_platform);
    rom->is_installed = rom_is_installed(state, rom, state->selected_platform);
    if (state->platforms[state->selected_platform].install_mode == INSTALL_FOLDER ||
        state->platforms[state->selected_platform].install_mode == INSTALL_PSX) {
        rom->is_downloaded = rom->is_downloaded || rom->is_installed;
    }
}

static bool validate_vita_install(const AppState *state, const char *title_id, char *missing, size_t missing_size) {
    if (missing_size > 0) missing[0] = '\0';
    if (!title_id || !title_id[0]) {
        snprintf(missing, missing_size, "unknown title id");
        return false;
    }

    char path[224];
    SceIoStat st;
    memset(&st, 0, sizeof(st));

    snprintf(path, sizeof(path), "%s/%s", state->vita_app_dir, title_id);
    if (sceIoGetstat(path, &st) < 0 || !SCE_S_ISDIR(st.st_mode)) {
        snprintf(missing, missing_size, "%s", path);
        return false;
    }

    snprintf(path, sizeof(path), "%s/%s/eboot.bin", state->vita_app_dir, title_id);
    if (sceIoGetstat(path, &st) < 0 || SCE_S_ISDIR(st.st_mode) || st.st_size <= 0) {
        snprintf(missing, missing_size, "%s", path);
        return false;
    }

    snprintf(path, sizeof(path), "%s/%s/sce_sys/param.sfo", state->vita_app_dir, title_id);
    if (sceIoGetstat(path, &st) < 0 || SCE_S_ISDIR(st.st_mode) || st.st_size <= 0) {
        snprintf(missing, missing_size, "%s", path);
        return false;
    }

    return true;
}

static bool is_vita_title_dir_name(const char *name) {
    if (!name || strlen(name) != 9) return false;
    for (int i = 0; i < 9; i++) {
        char c = name[i];
        bool digit = c >= '0' && c <= '9';
        bool upper = c >= 'A' && c <= 'Z';
        if (!digit && !upper) return false;
    }
    return true;
}

static bool livearea_entry_exists(const char *title_id) {
    char path[96];
    SceIoStat st;
    memset(&st, 0, sizeof(st));
    snprintf(path, sizeof(path), "ux0:appmeta/%s/param.sfo", title_id);
    if (sceIoGetstat(path, &st) >= 0 && !SCE_S_ISDIR(st.st_mode) && st.st_size > 0) return true;
    snprintf(path, sizeof(path), "ux0:appmeta/%s", title_id);
    return sceIoGetstat(path, &st) >= 0 && SCE_S_ISDIR(st.st_mode);
}

static void refresh_livearea_from_main(AppState *state) {
    char title_ids[MAX_REFRESH_TITLES][16];
    char patch_ids[MAX_REFRESH_TITLES][16];
    int title_count = 0;
    int patch_count = 0;

    state->busy = true;
    state->progress_current = 0;
    state->progress_total = 0;
    snprintf(state->status, sizeof(state->status), "Scanning Vita apps");
    snprintf(state->detail, sizeof(state->detail), "%.200s", state->vita_app_dir);
    redraw_now(state);

    SceUID dfd = sceIoDopen(state->vita_app_dir);
    if (dfd < 0) {
        snprintf(state->status, sizeof(state->status), "Refresh failed");
        snprintf(state->detail, sizeof(state->detail), "Cannot open %.180s: 0x%08X", state->vita_app_dir, (unsigned int)dfd);
        state->busy = false;
        return;
    }

    while (title_count < MAX_REFRESH_TITLES) {
        SceIoDirent ent;
        memset(&ent, 0, sizeof(ent));
        int res = sceIoDread(dfd, &ent);
        if (res <= 0) break;
        if (!SCE_S_ISDIR(ent.d_stat.st_mode)) continue;
        if (strcmp(ent.d_name, ".") == 0 || strcmp(ent.d_name, "..") == 0) continue;
        if (strcmp(ent.d_name, APP_TITLEID) == 0) continue;
        if (!is_vita_title_dir_name(ent.d_name)) continue;
        snprintf(title_ids[title_count], sizeof(title_ids[title_count]), "%s", ent.d_name);
        title_count++;
    }
    sceIoDclose(dfd);

    dfd = sceIoDopen(state->vita_patch_dir);
    if (dfd >= 0) {
        while (patch_count < MAX_REFRESH_TITLES) {
            SceIoDirent ent;
            memset(&ent, 0, sizeof(ent));
            int res = sceIoDread(dfd, &ent);
            if (res <= 0) break;
            if (!SCE_S_ISDIR(ent.d_stat.st_mode)) continue;
            if (strcmp(ent.d_name, ".") == 0 || strcmp(ent.d_name, "..") == 0) continue;
            if (!is_vita_title_dir_name(ent.d_name)) continue;
            snprintf(patch_ids[patch_count], sizeof(patch_ids[patch_count]), "%s", ent.d_name);
            patch_count++;
        }
        sceIoDclose(dfd);
    }

    if (title_count == 0 && patch_count == 0) {
        snprintf(state->status, sizeof(state->status), "Refresh LiveArea");
        snprintf(state->detail, sizeof(state->detail), "No Vita app/patch folders found");
        state->progress_current = 0;
        state->progress_total = 0;
        state->busy = false;
        return;
    }

    state->progress_total = title_count + patch_count;
    state->progress_current = 0;
    int ok = 0;
    int failed = 0;
    int last_error = 0;
    char last_failed[16] = "";
    for (int i = 0; i < title_count; i++) {
        state->progress_current = i + 1;
        snprintf(state->status, sizeof(state->status), "Refreshing LiveArea");
        snprintf(state->detail, sizeof(state->detail), "app %.9s  %d/%d  OK:%d  Fail:%d", title_ids[i], i + 1, title_count + patch_count, ok, failed);
        redraw_now(state);

        int res = 0;
        if (!livearea_entry_exists(title_ids[i])) {
            res = refresh_vita_livearea_for_title(state, title_ids[i]);
        }
        if (res >= 0) {
            ok++;
        } else {
            failed++;
            last_error = res;
            snprintf(last_failed, sizeof(last_failed), "%.9s", title_ids[i]);
            char log_line[180];
            snprintf(log_line, sizeof(log_line), "Refresh app failed: title=%s res=0x%08X", title_ids[i], (unsigned int)res);
            append_install_log(log_line);
        }
    }
    for (int i = 0; i < patch_count; i++) {
        state->progress_current = title_count + i + 1;
        snprintf(state->status, sizeof(state->status), "Refreshing LiveArea");
        snprintf(state->detail, sizeof(state->detail), "patch %.9s  %d/%d  OK:%d  Fail:%d", patch_ids[i], title_count + i + 1, title_count + patch_count, ok, failed);
        redraw_now(state);

        int res = refresh_vita_patch_for_title(state, patch_ids[i]);
        if (res >= 0) {
            ok++;
        } else {
            failed++;
            last_error = res;
            snprintf(last_failed, sizeof(last_failed), "%.9s", patch_ids[i]);
            char log_line[180];
            snprintf(log_line, sizeof(log_line), "Refresh patch failed: title=%s res=0x%08X", patch_ids[i], (unsigned int)res);
            append_install_log(log_line);
        }
    }

    if (failed == 0) {
        state->progress_current = state->progress_total;
        snprintf(state->status, sizeof(state->status), "Refresh complete");
        snprintf(state->detail, sizeof(state->detail), "LiveArea refreshed: %d / %d", ok, title_count + patch_count);
    } else {
        state->progress_current = state->progress_total;
        snprintf(state->status, sizeof(state->status), "Refresh complete with errors");
        snprintf(state->detail, sizeof(state->detail), "OK:%d Fail:%d Last:%s 0x%08X", ok, failed, last_failed, (unsigned int)last_error);
    }
    redraw_now(state);
    state->progress_current = 0;
    state->progress_total = 0;
    state->busy = false;
}
