#include "library_loader.h"
#include "config.h"
#include "cover_cache.h"
#include "http_buffer.h"
#include "net.h"
#include "platforms.h"
#include "romm_client.h"
#include "romm_parser.h"
#include "vita_fs.h"

static void add_platform_id(PlatformDef *platform, int id) {
    if (id < 0) return;
    for (int i = 0; i < platform->id_count; i++) {
        if (platform->ids[i] == id) return;
    }
    if (platform->id_count < MAX_PLATFORM_IDS) platform->ids[platform->id_count++] = id;
}

static void add_configured_platform(AppState *state, const PlatformDef *raw) {
    PlatformDef platform = *raw;
    configure_platform(state, &platform);
    if (!platform.known) return;

    for (int i = 0; i < state->catalog.count; i++) {
        if (strcmp(state->catalog.platforms[i].slug, platform.slug) == 0) {
            add_platform_id(&state->catalog.platforms[i], platform.id);
            return;
        }
    }

    if (state->catalog.count < MAX_PLATFORMS) {
        state->catalog.platforms[state->catalog.count++] = platform;
    }
}

static void load_platform_catalog(AppState *state, const char *json) {
    PlatformDef raw[MAX_PLATFORMS];
    int count = parse_platform_defs(json, raw, MAX_PLATFORMS);
    state->catalog.count = 0;
    state->catalog.selected = 0;
    for (int i = 0; i < count; i++) add_configured_platform(state, &raw[i]);
}

void load_platforms_and_roms(AppState *state) {
    state->busy = true;
    snprintf(state->status, sizeof(state->status), "Loading platforms");
    snprintf(state->detail, sizeof(state->detail), "GET /api/platforms");

    char url[360];
    long http = 0;
    build_api_url(url, sizeof(url), state, "/api/platforms");
    CURLcode c = http_get(state, url, http_response, &http);
    if (c != CURLE_OK || http != 200) {
        snprintf(state->status, sizeof(state->status), "RomM API error");
        snprintf(state->detail, sizeof(state->detail), "platforms: %s / %ld", curl_easy_strerror(c), http);
        state->busy = false;
        return;
    }
    load_platform_catalog(state, http_response);
    if (state->catalog.count <= 0) {
        snprintf(state->status, sizeof(state->status), "No platforms");
        snprintf(state->detail, sizeof(state->detail), "RomM returned no platforms");
        state->busy = false;
        return;
    }

    for (int p = 0; p < state->catalog.count; p++) {
        state->catalog.platforms[p].rom_count = 0;
        if (state->catalog.platforms[p].id < 0) continue;
        snprintf(state->status, sizeof(state->status), "Loading %s", state->catalog.platforms[p].label);

        state->catalog.platforms[p].rom_count = romm_get_platform_rom_count(state, &state->catalog.platforms[p], &http);
    }

    int out = 0;
    for (int i = 0; i < state->catalog.count; i++) {
        if (state->catalog.platforms[i].rom_count <= 0) continue;
        if (out != i) state->catalog.platforms[out] = state->catalog.platforms[i];
        out++;
    }
    state->catalog.count = out;
    if (state->catalog.selected >= state->catalog.count) state->catalog.selected = 0;
    if (state->catalog.count <= 0) {
        snprintf(state->status, sizeof(state->status), "No games");
        snprintf(state->detail, sizeof(state->detail), "No RomM platforms have games");
        state->busy = false;
        return;
    }

    for (int i = 0; i < state->catalog.count; i++) {
        load_cached_platform_icon(state, &state->catalog.platforms[i]);
    }

    char detail[160];
    snprintf(detail, sizeof(detail), "count=%d", state->catalog.count);
    log_event("platforms loaded", detail);

    snprintf(state->status, sizeof(state->status), "Ready");
    snprintf(state->detail, sizeof(state->detail), "Choose platform");
    state->view = VIEW_PLATFORMS;
    state->busy = false;
}

static void set_platform_game_detail(AppState *state, const PlatformDef *p) {
    if (p->support_level == SUPPORT_PATH_REQUIRED) {
        snprintf(state->detail, sizeof(state->detail), "Add platform.%s.path to config.txt", p->slug);
    } else if (p->support_level == SUPPORT_UNTESTED) {
        snprintf(state->detail, sizeof(state->detail), "Untested on PS Vita. Downloads to: %.120s", p->path);
    } else if (p->install_mode == INSTALL_FOLDER || p->install_mode == INSTALL_PSX) {
        snprintf(state->detail, sizeof(state->detail), "Downloads to: %.150s", p->path);
    } else {
        snprintf(state->detail, sizeof(state->detail), "RomM platform id: %d. Left/Right browse, X download, O back", p->id);
    }
}

typedef struct {
    AppState *state;
    PlatformDef platform;
    int platform_index;
    int generation;
} GameWorkerArgs;

enum {
    ROMM_PAGE_SIZE = 50
};

void lock_games(AppState *state) {
    if (state->library_worker.lock >= 0) sceKernelWaitSema(state->library_worker.lock, 1, NULL);
}

void unlock_games(AppState *state) {
    if (state->library_worker.lock >= 0) sceKernelSignalSema(state->library_worker.lock, 1);
}

static bool worker_generation_alive(AppState *state, int generation) {
    return generation == state->library_worker.generation;
}

static void fail_games_worker(AppState *state, int generation, const char *message) {
    lock_games(state);
    if (worker_generation_alive(state, generation)) {
        snprintf(state->status, sizeof(state->status), "ROM list error");
        snprintf(state->detail, sizeof(state->detail), "%s", message);
        state->library_worker.running = false;
        state->library_worker.loading_more = false;
    }
    unlock_games(state);
}

static void finish_games_worker(AppState *state, int generation, const PlatformDef *platform) {
    lock_games(state);
    if (worker_generation_alive(state, generation)) {
        state->library_worker.running = false;
        state->library_worker.loading_more = false;
        snprintf(state->status, sizeof(state->status), "%s games: %d", platform->label, state->library.count);
        set_platform_game_detail(state, platform);
    }
    unlock_games(state);
}

static void append_worker_roms(AppState *state, int generation, const PlatformDef *p, const RomItem *items, int count, int total_hint) {
    lock_games(state);
    if (!worker_generation_alive(state, generation)) {
        unlock_games(state);
        return;
    }
    for (int i = 0; i < count && state->library.count < MAX_ROMS; i++) {
        state->library.roms[state->library.count++] = items[i];
    }
    if (total_hint > 0) state->catalog.platforms[state->catalog.selected].rom_count = total_hint;
    snprintf(state->status, sizeof(state->status), "%s games: %d / %d", p->label, state->library.count, total_hint > 0 ? total_hint : state->library.count);
    set_platform_game_detail(state, p);
    unlock_games(state);
}

static int games_worker_entry(SceSize args_size, void *argp) {
    (void)args_size;
    GameWorkerArgs *args = NULL;
    if (argp) memcpy(&args, argp, sizeof(args));
    if (!args || !args->state) return 0;

    AppState *state = args->state;
    PlatformDef p = args->platform;
    int generation = args->generation;
    char *response = (char *)malloc(HTTP_RESPONSE_MAX + 1);
    if (!response) {
        fail_games_worker(state, generation, "Out of memory");
        free(args);
        return 0;
    }
    AppState *page = (AppState *)malloc(sizeof(AppState));
    if (!page) {
        fail_games_worker(state, generation, "Out of memory");
        free(response);
        free(args);
        return 0;
    }
    CURL *page_curl = curl_easy_init();

    int ids = p.id_count > 0 ? p.id_count : 1;
    for (int id_index = 0; id_index < ids && worker_generation_alive(state, generation); id_index++) {
        int platform_id = p.id_count > 0 ? p.ids[id_index] : p.id;
        int offset = 0;
        while (worker_generation_alive(state, generation)) {
            long http = 0;
            unsigned long long t0 = vita_now_ms();
            CURLcode c = romm_get_platform_roms_page_fast(state, page_curl, platform_id, ROMM_PAGE_SIZE, offset, response, &http);
            unsigned long long t1 = vita_now_ms();
            if (c != CURLE_OK || http != 200) {
                char detail[240];
                snprintf(detail, sizeof(detail), "slug=%s id=%d offset=%d http=%ld curl=%d http_ms=%llu", p.slug, platform_id, offset, http, c, t1 - t0);
                log_event("page failed", detail);
                break;
            }

            memset(page, 0, sizeof(*page));
            int added = parse_rom_items(response, p.slug, -1, page->library.roms, MAX_ROMS);
            if (offset == 0 && added > 0) {
                load_rom_metadata_from_detail(state, &page->library.roms[0]);
            }
            page->library.count = added;
            unsigned long long t2 = vita_now_ms();
            int total = extract_collection_count(response);
            append_worker_roms(state, generation, &p, page->library.roms, page->library.count, total);
            unsigned long long t3 = vita_now_ms();

            char detail[260];
            snprintf(detail, sizeof(detail), "slug=%s id=%d offset=%d added=%d total=%d bytes=%u http_ms=%llu parse_ms=%llu append_ms=%llu", p.slug, platform_id, offset, added, total, (unsigned int)strlen(response), t1 - t0, t2 - t1, t3 - t2);
            log_event("page loaded", detail);

            if (added <= 0 || added < ROMM_PAGE_SIZE || (total > 0 && offset + ROMM_PAGE_SIZE >= total)) break;
            offset += ROMM_PAGE_SIZE;
        }
    }

    if (page_curl) curl_easy_cleanup(page_curl);

    finish_games_worker(state, generation, &p);

    free(page);
    free(response);
    free(args);
    return 0;
}

static void start_games_worker(AppState *state, int platform_index, const PlatformDef *p) {
    state->library_worker.generation++;
    state->library_worker.running = true;
    state->library_worker.loading_more = true;
    state->library_worker.platform = platform_index;
    state->library_worker.id_index = 0;
    state->library_worker.offset = 0;
    state->library_worker.page_size = ROMM_PAGE_SIZE;

    GameWorkerArgs *args = (GameWorkerArgs *)malloc(sizeof(GameWorkerArgs));
    if (!args) {
        state->library_worker.running = false;
        state->library_worker.loading_more = false;
        snprintf(state->status, sizeof(state->status), "ROM list error");
        snprintf(state->detail, sizeof(state->detail), "Out of memory");
        return;
    }
    memset(args, 0, sizeof(*args));
    args->state = state;
    args->platform = *p;
    args->platform_index = platform_index;
    args->generation = state->library_worker.generation;

    SceUID thid = sceKernelCreateThread("vita_romm_games", games_worker_entry, 0x10000100, 256 * 1024, 0, 0, NULL);
    if (thid < 0) {
        free(args);
        state->library_worker.running = false;
        state->library_worker.loading_more = false;
        snprintf(state->status, sizeof(state->status), "ROM list error");
        snprintf(state->detail, sizeof(state->detail), "Worker create failed 0x%08X", (unsigned int)thid);
        return;
    }
    state->library_worker.thread = thid;
    GameWorkerArgs *thread_args = args;
    int start = sceKernelStartThread(thid, sizeof(thread_args), &thread_args);
    if (start < 0) {
        state->library_worker.generation++;
        state->library_worker.running = false;
        state->library_worker.loading_more = false;
        snprintf(state->status, sizeof(state->status), "ROM list error");
        snprintf(state->detail, sizeof(state->detail), "Worker start failed 0x%08X", (unsigned int)start);
        free(args);
    }
}

void load_games(AppState *state, int platform_index) {
    if (platform_index < 0 || platform_index >= state->catalog.count) return;
    PlatformDef *p = &state->catalog.platforms[platform_index];
    if (p->id < 0) {
        snprintf(state->status, sizeof(state->status), "%s not found", p->label);
        return;
    }
    state->library_worker.generation++;
    state->busy = false;
    snprintf(state->status, sizeof(state->status), "Loading %s games", p->label);
    state->library.count = 0;
    state->library.selected = 0;
    state->view = VIEW_GAMES;
    set_platform_game_detail(state, p);
    snprintf(state->detail, sizeof(state->detail), "Loading first 50...");
    start_games_worker(state, platform_index, p);
}
