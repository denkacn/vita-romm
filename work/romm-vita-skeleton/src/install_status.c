#include "installer.h"
#include "livearea.h"
#include "romm_client.h"
#include "util.h"
#include "vita_fs.h"
#include "zip_layout.h"

static bool psx_status_file_supported(const char *name) {
    return ends_with_ci(name, ".cue") || ends_with_ci(name, ".bin") ||
        ends_with_ci(name, ".pbp") || ends_with_ci(name, ".chd") ||
        ends_with_ci(name, ".iso") || ends_with_ci(name, ".img") ||
        ends_with_ci(name, ".ccd") || ends_with_ci(name, ".sub") ||
        ends_with_ci(name, ".m3u") || ends_with_ci(name, ".zip");
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
    const PlatformDef *p = &state->catalog.platforms[platform_index];
    if (p->install_mode != INSTALL_PSX || !p->path_configured || !p->path[0]) return false;

    char folder[180];
    char path[384];
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

void psx_rom_file_destination(const AppState *state, const RomItem *rom, const RomFileItem *file_item, int platform_index, char *path, size_t path_size) {
    const PlatformDef *p = &state->catalog.platforms[platform_index];
    char file[180];
    copy_value(file, sizeof(file), basename_ptr(file_item->name));
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
        if (!psx_status_file_supported(files[i].name)) continue;
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

    char path[384];
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
    if (state->catalog.platforms[platform_index].install_mode == INSTALL_PSX) {
        return psx_expected_files_status(state, rom, platform_index, false) ||
            psx_folder_has_playable_content(state, rom, platform_index);
    }
    char path[384];
    rom_download_path(state, rom, platform_index, path, sizeof(path));
    return usable_file_exists(path);
}

static bool rom_is_installed(AppState *state, RomItem *rom, int platform_index) {
    const PlatformDef *p = &state->catalog.platforms[platform_index];
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
    copy_value(file, sizeof(file), rom->file_name[0] ? rom->file_name : rom->name);
    sanitize_filename(file);
    if (p->install_mode == INSTALL_ADRENALINE_PSP && (ends_with_ci(file, ".iso") || ends_with_ci(file, ".cso"))) {
        char path[384];
        snprintf(path, sizeof(path), "%s/%s", state->psp_install_dir, file);
        return usable_file_exists(path);
    }
    if ((p->install_mode == INSTALL_ADRENALINE_PSP || p->install_mode == INSTALL_PSX) && ends_with_ci(file, ".pbp")) {
        char path[384];
        char folder[80];
        make_folder_name_from_file(file, folder, sizeof(folder));
        snprintf(path, sizeof(path), "%s/%s/EBOOT.PBP", p->install_mode == INSTALL_PSX ? state->psx_pbp_dir : state->psp_game_dir, folder);
        return usable_file_exists(path);
    }
    if (p->install_mode == INSTALL_PSX && psx_file_is_direct_install(file)) {
        char path[384];
        snprintf(path, sizeof(path), "%s/%s", p->path, file);
        if (usable_file_exists(path)) return true;

        return psx_folder_has_playable_content(state, rom, platform_index);
    }
    if (p->install_mode == INSTALL_FOLDER) {
        char path[384];
        snprintf(path, sizeof(path), "%s/%s", p->path, file);
        return usable_file_exists(path);
    }
    if (!ends_with_ci(file, ".zip")) return false;

    char zip_path[384];
    rom_download_path(state, rom, platform_index, zip_path, sizeof(zip_path));
    if (!usable_file_exists(zip_path)) return false;

    int err = 0;
    zip_t *archive = zip_open(zip_path, ZIP_RDONLY, &err);
    if (!archive) return false;
    PspZipHint hint;
    bool has_hint = (p->install_mode == INSTALL_ADRENALINE_PSP || p->install_mode == INSTALL_PSX) && find_psp_zip_hint(archive, &hint);
    if (has_hint && hint.folder[0]) {
        zip_close(archive);
        char path[384];
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
        copy_value(file, sizeof(file), basename_ptr(name));
        sanitize_filename(file);
        char path[384];
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

void update_rom_status(AppState *state, int index) {
    if (index < 0 || index >= state->library.count) return;
    RomItem *rom = &state->library.roms[index];
    rom->is_downloaded = rom_is_downloaded(state, rom, state->catalog.selected);
    rom->is_installed = rom_is_installed(state, rom, state->catalog.selected);
    if (state->catalog.platforms[state->catalog.selected].install_mode == INSTALL_FOLDER ||
        state->catalog.platforms[state->catalog.selected].install_mode == INSTALL_PSX) {
        rom->is_downloaded = rom->is_downloaded || rom->is_installed;
    }
}

bool validate_vita_install(const AppState *state, const char *title_id, char *missing, size_t missing_size) {
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



