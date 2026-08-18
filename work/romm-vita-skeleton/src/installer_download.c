#include "installer_internal.h"

void installer_build_rom_content_url(AppState *state, const RomItem *rom, const char *file_name, char *url, size_t url_size) {
    char file[384];
    char encoded[420];
    char api_path[560];
    copy_value(file, sizeof(file), file_name && file_name[0] ? file_name : (rom->file_name[0] ? rom->file_name : rom->name));
    url_encode(encoded, sizeof(encoded), file);
    snprintf(api_path, sizeof(api_path), "/api/roms/%d/content/%s", rom->id, encoded);
    build_api_url(url, url_size, state, api_path);
}

void installer_build_rom_file_content_url(AppState *state, int file_id, const char *file_name, char *url, size_t url_size) {
    char encoded[420];
    char api_path[560];
    url_encode(encoded, sizeof(encoded), file_name);
    snprintf(api_path, sizeof(api_path), "/api/roms/%d/files/content/%s", file_id, encoded);
    build_api_url(url, url_size, state, api_path);
}

bool installer_download_rom_file_to(AppState *state, const RomItem *rom, const RomFileItem *file_item, const char *dst) {
    const char *source_name = file_item->name;
    const char *maybe_url = file_item->url;
    long long expected_size = file_item->size_bytes;
    DownloadJob job;
    memset(&job, 0, sizeof(job));
    job.platform_index = state->catalog.selected;
    job.rom_index = state->library.selected;
    snprintf(job.destination, sizeof(job.destination), "%s", dst);
    if (usable_file_exists(job.destination)) {
        snprintf(state->status, sizeof(state->status), "Using installed file");
        snprintf(state->detail, sizeof(state->detail), "%.230s", job.destination);
        redraw_now(state);
        return true;
    }

    char url[640];
    char part_path[420];
    long http = 0;
    if (maybe_url && maybe_url[0]) {
        make_absolute_url(url, sizeof(url), state, maybe_url);
    } else if (file_item->id > 0) {
        installer_build_rom_file_content_url(state, file_item->id, source_name, url, sizeof(url));
    } else {
        installer_build_rom_content_url(state, rom, source_name, url, sizeof(url));
    }
    snprintf(job.source_url, sizeof(job.source_url), "%s", url);
    mkdir_parent_for_file(job.destination);
    snprintf(part_path, sizeof(part_path), "%s.part", job.destination);
    remove_path_recursive(part_path);

    snprintf(state->status, sizeof(state->status), "Downloading");
    snprintf(state->detail, sizeof(state->detail), "%.180s", source_name);
    redraw_now(state);
    CURLcode c = http_download(state, url, part_path, &http);
    if (c == CURLE_OK && http == 200) {
        SceIoStat st;
        memset(&st, 0, sizeof(st));
        long long actual_size = 0;
        if (sceIoGetstat(part_path, &st) >= 0) actual_size = st.st_size;
        if ((expected_size > 0 && actual_size > expected_size + (1024 * 1024) && actual_size > expected_size * 4) ||
            (ends_with_ci(source_name, ".cue") && actual_size > 8 * 1024 * 1024)) {
            char detail[520];
            snprintf(detail, sizeof(detail), "source=%.160s expected=%lld actual=%lld url=%.240s", source_name, expected_size, actual_size, job.source_url);
            log_event("download size mismatch", detail);
            remove_path_recursive(part_path);
            snprintf(state->status, sizeof(state->status), "Download rejected");
            snprintf(state->detail, sizeof(state->detail), "Size mismatch: %.120s", source_name);
            return false;
        }
        remove_path_recursive(job.destination);
        if (sceIoRename(part_path, job.destination) >= 0) return true;
    }

    remove_path_recursive(part_path);
    snprintf(state->status, sizeof(state->status), "Download failed");
    snprintf(state->detail, sizeof(state->detail), "%s / HTTP %ld", curl_easy_strerror(c), http);
    return false;
}
