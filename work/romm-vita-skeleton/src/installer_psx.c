#include "installer_internal.h"

int installer_install_psx_multi_files(AppState *state, RomItem *rom, PlatformDef *p) {
    (void)p;
    RomFileItem files[24];
    int count = load_rom_file_list(state, rom, files, 24);
    int target_count = 0;
    long long target_size = 0;
    for (int i = 0; i < count; i++) {
        if (psx_file_is_direct_install(files[i].name)) {
            target_count++;
            if (files[i].size_bytes > 0) target_size += files[i].size_bytes;
        }
    }
    if (target_count <= 0) return 0;
    if (target_size > 0) rom->size_bytes = target_size;

    char folder[180];
    rom_folder_name(rom, folder, sizeof(folder));
    int ok = 0;
    int total = 0;
    char detail[420];
    snprintf(detail, sizeof(detail), "rom=%d files=%d targets=%d folder=%s", rom->id, count, target_count, folder);
    log_event("install started", detail);
    for (int i = 0; i < count; i++) {
        if (!psx_file_is_direct_install(files[i].name)) continue;
        char file[180];
        copy_value(file, sizeof(file), basename_ptr(files[i].name));
        sanitize_filename(file);
        char dst[384];
        psx_rom_file_destination(state, rom, &files[i], state->catalog.selected, dst, sizeof(dst));
        total++;
        state->progress.current = total;
        state->progress.total = target_count;
        snprintf(state->status, sizeof(state->status), "Downloading file %d/%d", total, target_count);
        snprintf(state->detail, sizeof(state->detail), "%.180s", file);
        redraw_now(state);
        snprintf(detail, sizeof(detail), "file=%d/%d id=%d source=%.150s size=%lld url=%s dst=%.120s", total, target_count, files[i].id, files[i].name, files[i].size_bytes, files[i].url[0] ? "api" : (files[i].id > 0 ? "file-content" : "rom-content"), dst);
        log_event("download started", detail);
        if (installer_download_rom_file_to(state, rom, &files[i], dst)) ok++;
    }
    state->progress.current = 0;
    state->progress.total = 0;
    if (ok == total && total > 0) {
        rom->is_downloaded = true;
        rom->is_installed = true;
        snprintf(state->status, sizeof(state->status), "Installed");
        snprintf(state->detail, sizeof(state->detail), "PSX files: %d installed", ok);
        snprintf(detail, sizeof(detail), "rom=%d ok=%d total=%d folder=%s", rom->id, ok, total, folder);
        log_event("install completed", detail);
        return 1;
    }
    snprintf(state->status, sizeof(state->status), "Install incomplete");
    snprintf(state->detail, sizeof(state->detail), "PSX files: %d / %d", ok, total);
    snprintf(detail, sizeof(detail), "rom=%d ok=%d total=%d no fallback", rom->id, ok, total);
    log_event("install incomplete", detail);
    return 1;
}
