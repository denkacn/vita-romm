#include "livearea.h"
#include "util.h"
#include "zip_layout.h"

bool extension_matches_platform(const char *name, const PlatformDef *p) {
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

bool psx_file_is_direct_install(const char *name) {
    return psx_file_is_supported(name) && !ends_with_ci(name, ".zip");
}

void platform_cache_dir(const PlatformDef *p, char *path, size_t path_size) {
    snprintf(path, path_size, "%s/%s", DOWNLOAD_DIR, p->slug[0] ? p->slug : "custom");
}

void rom_folder_name(const RomItem *rom, char *folder, size_t folder_size) {
    copy_value(folder, folder_size, rom->name[0] ? rom->name : rom->file_name);
    char *dot = strrchr(folder, '.');
    if (dot) *dot = '\0';
    sanitize_filename(folder);
    if (!folder[0]) snprintf(folder, folder_size, "rom_%d", rom->id);
}

void rom_download_path(const AppState *state, const RomItem *rom, int platform_index, char *path, size_t path_size) {
    const PlatformDef *p = &state->catalog.platforms[platform_index];
    char file[180];
    copy_value(file, sizeof(file), rom->file_name[0] ? rom->file_name : rom->name);
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


