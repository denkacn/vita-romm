#include "romm_client.h"
#include "config.h"
#include "http_buffer.h"
#include "net.h"
#include "romm_parser.h"
#include "util.h"
#include "vita_fs.h"

// /api/roms defaults with_char_index and with_filter_values to true, which can
// push a 50-item page past HTTP_RESPONSE_MAX. The older with_extra and
// with_rom_id_index names are kept for servers that still read them.
#define ROMM_LIST_SLIM_PARAMS \
    "&with_char_index=false&with_filter_values=false&with_files=false" \
    "&with_extra=false&with_rom_id_index=false"

static CURLcode get_platform_roms_page_into(AppState *state, int platform_id, const char *slug, int limit, int offset, char *response, long *http_out) {
    char url[480];
    char path[260];
    const char *formats[] = {
        "/api/roms?platform_ids=%d&limit=%d&offset=%d" ROMM_LIST_SLIM_PARAMS,
        "/api/roms?platform_id=%d&limit=%d&offset=%d" ROMM_LIST_SLIM_PARAMS,
        "/api/roms?platform=%d&limit=%d&offset=%d" ROMM_LIST_SLIM_PARAMS
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
            char detail[360];
            snprintf(detail, sizeof(detail),
                "slug=%s id=%d query=%d http=%ld total=%d objects=%d matched=%d",
                slug, platform_id, i, *http_out, total, objects, matched);
            log_event("platform probe", detail);
        }
        if (matched > 0 || objects == 0 || total == 0) return last_c;
    }

    *http_out = last_http;
    return last_c;
}

static CURLcode get_platform_roms_page(AppState *state, int platform_id, const char *slug, int limit, int offset, long *http_out) {
    return get_platform_roms_page_into(state, platform_id, slug, limit, offset, http_response, http_out);
}

CURLcode romm_get_platform_roms_page_fast(AppState *state, CURL *curl, int platform_id, int limit, int offset, char *response, long *http_out) {
    char url[480];
    char path[260];
    snprintf(path, sizeof(path),
        "/api/roms?platform_ids=%d&limit=%d&offset=%d&order_by=name&order_dir=asc" ROMM_LIST_SLIM_PARAMS,
        platform_id, limit, offset);
    build_api_url(url, sizeof(url), state, path);
    return http_get_reuse(state, curl, url, response, http_out);
}

unsigned long long vita_now_ms(void) {
    return (unsigned long long)(sceKernelGetProcessTimeWide() / 1000ULL);
}

int romm_get_platform_rom_count(AppState *state, const PlatformDef *platform, long *http_out) {
    int total = 0;
    int ids = platform->id_count > 0 ? platform->id_count : 1;
    for (int i = 0; i < ids; i++) {
        int platform_id = platform->id_count > 0 ? platform->ids[i] : platform->id;
        // Only the page's total is needed here, so fetch a single item.
        CURLcode c = get_platform_roms_page(state, platform_id, platform->slug, 1, 0, http_out);
        if (c != CURLE_OK || *http_out != 200) continue;
        int api_total = extract_collection_count(http_response);
        int page_count = count_roms_for_platform(http_response, platform_id);
        if (api_total > 0) total += api_total;
        else total += page_count;
    }
    char detail[320];
    snprintf(detail, sizeof(detail), "slug=%s label=%s id=%d ids=%d rom_count=%d",
        platform->slug, platform->label, platform->id, platform->id_count, total);
    log_event("platform count", detail);
    return total;
}

void make_absolute_url(char *dst, size_t dst_size, const AppState *state, const char *maybe_path) {
    if (starts_with(maybe_path, "http://") || starts_with(maybe_path, "https://")) {
        snprintf(dst, dst_size, "%s", maybe_path);
    } else if (starts_with(maybe_path, "/")) {
        build_api_url(dst, dst_size, state, maybe_path);
    } else {
        char path[384];
        snprintf(path, sizeof(path), "/%s", maybe_path);
        build_api_url(dst, dst_size, state, path);
    }
    escape_url_spaces(dst, dst_size);
}

static bool get_rom_detail_json(AppState *state, int rom_id, char *response, long *http_out) {
    char api_path[64];
    char url[360];

    snprintf(api_path, sizeof(api_path), "/api/roms/%d", rom_id);
    build_api_url(url, sizeof(url), state, api_path);
    return http_get(state, url, response, http_out) == CURLE_OK && *http_out == 200;
}

static void log_rom_detail_failure(int rom_id, long http) {
    char detail[160];
    snprintf(detail, sizeof(detail), "rom=%d http=%ld", rom_id, http);
    log_event("rom detail failed", detail);
}

bool load_rom_metadata_from_detail(AppState *state, RomItem *rom) {
    long http = 0;
    char *response = (char *)malloc(HTTP_RESPONSE_MAX + 1);
    if (!response) return false;

    if (!get_rom_detail_json(state, rom->id, response, &http)) {
        log_rom_detail_failure(rom->id, http);
        free(response);
        return false;
    }

    long long size = parse_rom_total_size(response);
    if (size > 0) rom->size_bytes = size;

    if (find_cover_url(response, response + strlen(response), rom->cover_url, sizeof(rom->cover_url))) {
        char detail[320];
        snprintf(detail, sizeof(detail), "rom=%d size=%lld url=%.210s",
            rom->id, rom->size_bytes, rom->cover_url);
        log_event("rom detail loaded", detail);
        free(response);
        return true;
    }

    char detail[180];
    snprintf(detail, sizeof(detail), "rom=%d size=%lld name=%.90s", rom->id, rom->size_bytes, rom->name);
    log_event("rom cover missing", detail);
    free(response);
    return size > 0;
}

int load_rom_file_list(AppState *state, const RomItem *rom, RomFileItem *files, int max_files) {
    long http = 0;

    if (!get_rom_detail_json(state, rom->id, http_response, &http)) {
        char detail[160];
        snprintf(detail, sizeof(detail), "rom=%d http=%ld", rom->id, http);
        log_event("rom files failed", detail);
        return 0;
    }
    return parse_rom_files(http_response, files, max_files);
}
