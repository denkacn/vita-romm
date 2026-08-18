#include "installer_internal.h"
#include "vita_paths.h"

typedef struct {
    char file[180];
    char url[460];
    char path[384];
    char part_path[420];
} InstallPlan;

static void set_install_status(AppState *state, const char *status, const char *detail) {
    snprintf(state->status, sizeof(state->status), "%s", status);
    snprintf(state->detail, sizeof(state->detail), "%.255s", detail ? detail : "");
}

static void log_install_state(const char *event, const PlatformDef *platform, const RomItem *rom, const AppState *state) {
    char detail[320];
    snprintf(detail, sizeof(detail), "platform=%s rom=%d downloaded=%d installed=%d status=%.80s",
        platform->slug, rom->id, rom->is_downloaded, rom->is_installed, state->status);
    log_event(event, detail);
}

static void platform_install_dirs(const AppState *state, const PlatformDef *platform, char *download_dir, size_t download_size, const char **install_dir) {
    if (platform->install_mode == INSTALL_ADRENALINE_PSP) {
        snprintf(download_dir, download_size, "%s", state->psp_download_dir);
        *install_dir = state->psp_install_dir;
    } else if (platform->install_mode == INSTALL_FOLDER) {
        snprintf(download_dir, download_size, "%s", platform->path);
        *install_dir = platform->path;
    } else if (platform->install_mode == INSTALL_PSX) {
        platform_cache_dir(platform, download_dir, download_size);
        *install_dir = platform->path;
    } else {
        snprintf(download_dir, download_size, "%s", state->vita_download_dir);
        *install_dir = state->vita_download_dir;
    }
}

static void build_install_plan(AppState *state, const PlatformDef *platform, const RomItem *rom, InstallPlan *plan) {
    memset(plan, 0, sizeof(*plan));
    copy_value(plan->file, sizeof(plan->file), rom->file_name[0] ? rom->file_name : rom->name);
    sanitize_filename(plan->file);

    char download_dir[180];
    const char *install_dir = state->vita_download_dir;
    platform_install_dirs(state, platform, download_dir, sizeof(download_dir), &install_dir);

    if (platform->install_mode == INSTALL_ADRENALINE_PSP && (ends_with_ci(plan->file, ".iso") || ends_with_ci(plan->file, ".cso"))) {
        vita_path_join(plan->path, sizeof(plan->path), install_dir, plan->file);
    } else if ((platform->install_mode == INSTALL_ADRENALINE_PSP || platform->install_mode == INSTALL_PSX) && ends_with_ci(plan->file, ".pbp")) {
        char folder[80];
        make_folder_name_from_file(plan->file, folder, sizeof(folder));
        vita_path_join3(plan->path, sizeof(plan->path), platform->install_mode == INSTALL_PSX ? state->psx_pbp_dir : state->psp_game_dir, folder, "EBOOT.PBP");
    } else if (platform->install_mode == INSTALL_FOLDER || platform->install_mode == INSTALL_PSX) {
        vita_path_join(plan->path, sizeof(plan->path), ends_with_ci(plan->file, ".zip") ? download_dir : install_dir, plan->file);
    } else {
        vita_path_join(plan->path, sizeof(plan->path), download_dir, plan->file);
    }

    char encoded[240];
    char api_path[300];
    url_encode(encoded, sizeof(encoded), plan->file);
    snprintf(api_path, sizeof(api_path), "/api/roms/%d/content/%s", rom->id, encoded);
    build_api_url(plan->url, sizeof(plan->url), state, api_path);

    mkdir_parent_for_file(plan->path);
    vita_path_part(plan->part_path, sizeof(plan->part_path), plan->path);
}

static bool download_or_use_cache(AppState *state, const RomItem *rom, const PlatformDef *platform, const InstallPlan *plan, CURLcode *curl_out, long *http_out) {
    *curl_out = CURLE_OK;
    *http_out = 200;

    if (usable_file_exists(plan->path)) {
        set_install_status(state, "Using cached file", plan->path);
        redraw_now(state);
        return true;
    }

    remove_path_recursive(plan->part_path);
    char detail[260];
    snprintf(detail, sizeof(detail), "%s -> %s", rom->name, platform->slug);
    set_install_status(state, "Downloading", detail);
    redraw_now(state);

    *curl_out = http_download(state, plan->url, plan->part_path, http_out);
    if (*curl_out != CURLE_OK || *http_out != 200) return false;

    remove_path_recursive(plan->path);
    if (sceIoRename(plan->part_path, plan->path) < 0) {
        *curl_out = CURLE_WRITE_ERROR;
        return false;
    }
    return true;
}

static void set_vita_zip_status(AppState *state, const InstallResult *result) {
    char missing[224];
    if (!validate_vita_install(state, result->title_id, missing, sizeof(missing))) {
        set_install_status(state, "Installed, app invalid", missing);
        char detail[520];
        snprintf(detail, sizeof(detail), "title=%s files=%d matched=%d skipped=%d missing=%.180s msg=%.120s",
            result->title_id, result->installed, result->matched, result->skipped, missing, result->message);
        log_event("install invalid", detail);
        return;
    }

    if (!state->auto_refresh_livearea) {
        char detail[180];
        snprintf(detail, sizeof(detail), "App OK: %s. Press Triangle or use VitaShell Refresh LiveArea", result->title_id);
        set_install_status(state, "Installed", detail);
        return;
    }

    set_install_status(state, "Refreshing LiveArea", result->title_id);
    redraw_now(state);
    int promote = refresh_vita_livearea_for_title(state, result->title_id);
    char detail[220];
    if (promote >= 0) {
        snprintf(detail, sizeof(detail), "Files: %d, skipped: %d. LiveArea refreshed: %s", result->installed, result->skipped, result->title_id);
        set_install_status(state, "Installed", detail);
    } else {
        snprintf(detail, sizeof(detail), "Files: %d, skipped: %d. Try VitaShell refresh. Promoter: 0x%08X", result->installed, result->skipped, (unsigned int)promote);
        set_install_status(state, "Installed, refresh failed", detail);
    }
}

static InstallResult install_zip(AppState *state, const PlatformDef *platform, const RomItem *rom, const char *path) {
    InstallResult result;
    memset(&result, 0, sizeof(result));

    set_install_status(state, "Installing ZIP", path);
    redraw_now(state);

    int installed = 0;
    if (platform->install_mode == INSTALL_PSX) {
        installed = installer_install_folder_zip(state, path, state->catalog.selected, rom, &result.matched, &result.skipped, result.message, sizeof(result.message));
    } else {
        installed = installer_install_content_zip(state, path, state->catalog.selected, &result.matched, &result.skipped, result.title_id, sizeof(result.title_id), result.message, sizeof(result.message));
    }
    result.installed = installed;

    if (installed > 0) {
        if (platform->install_mode == INSTALL_VITA && result.title_id[0]) {
            set_vita_zip_status(state, &result);
        } else {
            char detail[96];
            snprintf(detail, sizeof(detail), "Files: %d, skipped: %d", installed, result.skipped);
            set_install_status(state, "Installed", detail);
        }
        return result;
    }

    if (installed == 0 && result.matched > 0) {
        char detail[220];
        snprintf(detail, sizeof(detail), "Matched:%d skipped:%d fail:%.130s", result.matched, result.skipped, result.message[0] ? result.message : "Check ZIP compression/encryption");
        set_install_status(state, "ZIP extract failed", detail);
    } else if (installed == 0) {
        char detail[240];
        snprintf(detail, sizeof(detail), "Unknown ZIP layout. Saved: %.190s", path);
        set_install_status(state, "Downloaded, not installed", detail);
    } else {
        char detail[240];
        snprintf(detail, sizeof(detail), "Saved: %.220s", path);
        set_install_status(state, "ZIP open failed", detail);
    }
    return result;
}

static void handle_downloaded_file(AppState *state, const PlatformDef *platform, const RomItem *rom, const InstallPlan *plan) {
    if (ends_with_ci(plan->path, ".zip")) {
        if (platform->install_mode == INSTALL_FOLDER) {
            set_install_status(state, "Installed", plan->path);
        } else {
            install_zip(state, platform, rom, plan->path);
        }
        return;
    }

    set_install_status(state, platform->install_mode == INSTALL_VITA ? "Downloaded" : "Installed", plan->path);
}

void download_selected_rom(AppState *state) {
    if (state->library.count <= 0) return;

    PlatformDef *platform = &state->catalog.platforms[state->catalog.selected];
    RomItem *rom = &state->library.roms[state->library.selected];

    if (platform->support_level == SUPPORT_PATH_REQUIRED) {
        char detail[220];
        snprintf(detail, sizeof(detail), "Add platform.%s.path to %s", platform->slug, CONFIG_PATH);
        set_install_status(state, "Path required", detail);
        return;
    }

    state->busy = true;
    char detail[260];
    snprintf(detail, sizeof(detail), "platform=%s rom=%d name=%.150s", platform->slug, rom->id, rom->name);
    log_event("install started", detail);

    if (platform->install_mode == INSTALL_PSX && installer_install_psx_multi_files(state, rom, platform) != 0) {
        update_rom_status(state, state->library.selected);
        log_install_state("install completed", platform, rom, state);
        state->busy = false;
        return;
    }

    InstallPlan plan;
    build_install_plan(state, platform, rom, &plan);

    CURLcode curl = CURLE_OK;
    long http = 0;
    if (download_or_use_cache(state, rom, platform, &plan, &curl, &http)) {
        handle_downloaded_file(state, platform, rom, &plan);
    } else {
        char error[180];
        snprintf(error, sizeof(error), "%s / HTTP %ld", curl_easy_strerror(curl), http);
        set_install_status(state, "Download failed", error);
    }

    update_rom_status(state, state->library.selected);
    log_install_state("install completed", platform, rom, state);
    state->busy = false;
}
