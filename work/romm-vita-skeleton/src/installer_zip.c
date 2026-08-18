#include "installer_internal.h"

int installer_copy_zip_entry(zip_t *archive, zip_uint64_t index, const char *dst) {
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

int installer_install_content_zip(AppState *state, const char *zip_path, int platform_index, int *matched, int *skipped, char *vita_title_id, size_t vita_title_id_size, char *first_failed, size_t first_failed_size) {
    int err = 0;
    zip_t *archive = zip_open(zip_path, ZIP_RDONLY, &err);
    if (!archive) return -1;

    int installed = 0;
    *matched = 0;
    *skipped = 0;
    if (first_failed_size > 0) first_failed[0] = '\0';
    VitaZipHint hint;
    bool has_hint = state->catalog.platforms[platform_index].install_mode == INSTALL_VITA && find_vita_zip_hint(archive, &hint);
    PspZipHint psp_hint;
    bool has_psp_hint = state->catalog.platforms[platform_index].install_mode == INSTALL_ADRENALINE_PSP && find_psp_zip_hint(archive, &psp_hint);
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
        if (state->catalog.platforms[platform_index].install_mode == INSTALL_VITA && vita_title_id_size > 0 && vita_title_id[0] == '\0') {
            get_vita_title_id_for_entry(name, has_hint ? &hint : NULL, vita_title_id, vita_title_id_size);
        }

        char dst[384];
        if (!map_zip_entry_to_install_path(state, platform_index, name, has_hint ? &hint : NULL, has_psp_hint ? &psp_hint : NULL, dst, sizeof(dst))) {
            (*skipped)++;
            continue;
        }

        (*matched)++;
        int copy_res = installer_copy_zip_entry(archive, i, dst);
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
        char detail[520];
        snprintf(detail, sizeof(detail), "%s", first_failed);
        log_event("zip copy failed", detail);
    }
    return installed;
}

int installer_install_folder_zip(AppState *state, const char *zip_path, int platform_index, const RomItem *rom, int *matched, int *skipped, char *first_failed, size_t first_failed_size) {
    int err = 0;
    zip_t *archive = zip_open(zip_path, ZIP_RDONLY, &err);
    if (!archive) return -1;

    PlatformDef *p = &state->catalog.platforms[platform_index];
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
        copy_value(file, sizeof(file), basename_ptr(entry));
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
        int copy_res = installer_copy_zip_entry(archive, i, dst);
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
