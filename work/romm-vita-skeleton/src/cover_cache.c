#include "cover_cache.h"
#include "config.h"
#include "library_loader.h"
#include "net.h"
#include "romm_client.h"
#include "romm_parser.h"
#include "vita_fs.h"

void load_cached_platform_icon(AppState *state, PlatformDef *p) {
    if (p->known || p->icon || !p->slug[0]) return;
    const char *exts[] = { "png", "jpg", "jpeg" };
    char path[160];
    char url_path[120];
    char url[360];
    long http = 0;

    for (int i = 0; i < 3; i++) {
        snprintf(path, sizeof(path), "%s/%s.%s", PLATFORM_ICON_CACHE_DIR, p->slug, exts[i]);
        if (!usable_file_exists(path)) {
            snprintf(url_path, sizeof(url_path), "/assets/platforms/%s.%s", p->slug, exts[i]);
            build_api_url(url, sizeof(url), state, url_path);
            snprintf(state->status, sizeof(state->status), "Loading platform icon");
            snprintf(state->detail, sizeof(state->detail), "%s", p->label);
            redraw_now(state);
            CURLcode c = http_download(state, url, path, &http);
            if (c != CURLE_OK || http != 200) {
                sceIoRemove(path);
                continue;
            }
        }
        p->icon = vita2d_load_PNG_file(path);
        if (!p->icon) p->icon = vita2d_load_JPEG_file(path);
        if (p->icon) {
            snprintf(p->icon_path, sizeof(p->icon_path), "%s", path);
            return;
        }
        sceIoRemove(path);
    }
}

void load_current_cover_from_cache(AppState *state) {
    if (state->library.count <= 0) return;
    RomItem *rom = &state->library.roms[state->library.selected];
    if (rom->cover) return;

    SceIoStat st;
    if (sceIoGetstat(rom->cover_path, &st) < 0 || st.st_size <= 0) return;
    rom->cover = vita2d_load_PNG_file(rom->cover_path);
    if (!rom->cover) rom->cover = vita2d_load_JPEG_file(rom->cover_path);
    if (!rom->cover) sceIoRemove(rom->cover_path);
}

typedef struct {
    AppState *state;
    int rom_index;
    CoverJob job;
} CoverWorkerArgs;

static int cover_worker_entry(SceSize args_size, void *argp) {
    (void)args_size;
    CoverWorkerArgs *args = NULL;
    if (argp) memcpy(&args, argp, sizeof(args));
    if (!args || !args->state) return 0;

    AppState *state = args->state;
    CoverJob job = args->job;
    char cover_url[320];
    char cover_path[160];
    snprintf(cover_url, sizeof(cover_url), "%s", job.cover_url);
    snprintf(cover_path, sizeof(cover_path), "%s", job.cache_path);

    char *response = NULL;
    if (!cover_url[0]) {
        response = (char *)malloc(HTTP_RESPONSE_MAX + 1);
        if (response) {
            char api_path[64];
            char url[360];
            long http = 0;
            snprintf(api_path, sizeof(api_path), "/api/roms/%d", job.rom_id);
            build_api_url(url, sizeof(url), state, api_path);
            CURLcode c = http_get(state, url, response, &http);
            if (c == CURLE_OK && http == 200) {
                find_cover_url(response, response + strlen(response), cover_url, sizeof(cover_url));
                lock_games(state);
                if (job.generation == state->cover_worker.generation &&
                    args->rom_index >= 0 && args->rom_index < state->library.count &&
                    state->library.roms[args->rom_index].id == job.rom_id &&
                    cover_url[0]) {
                    snprintf(state->library.roms[args->rom_index].cover_url, sizeof(state->library.roms[args->rom_index].cover_url), "%s", cover_url);
                }
                unlock_games(state);
            }
        }
    }

    SceIoStat st;
    if (cover_url[0] && sceIoGetstat(cover_path, &st) < 0) {
        char url[420];
        char part_path[192];
        long http = 0;
        make_absolute_url(url, sizeof(url), state, cover_url);
        snprintf(part_path, sizeof(part_path), "%s.part", cover_path);
        remove_path_recursive(part_path);
        mkdir_parent_for_file(cover_path);
        CURLcode c = http_download_quiet(state, url, part_path, &http);
        if (c == CURLE_OK && http == 200) {
            remove_path_recursive(cover_path);
            if (sceIoRename(part_path, cover_path) < 0) remove_path_recursive(part_path);
        } else {
            remove_path_recursive(part_path);
            char detail[420];
            snprintf(detail, sizeof(detail), "rom=%d http=%ld curl=%d url=%.240s", job.rom_id, http, c, url);
            log_event("cover failed", detail);
        }
    }

    lock_games(state);
    if (job.generation == state->cover_worker.generation) state->cover_worker.running = false;
    unlock_games(state);
    if (response) free(response);
    free(args);
    return 0;
}

static void start_current_cover_worker(AppState *state) {
    if (state->library.count <= 0 || state->library.selected < 0 || state->library.selected >= state->library.count) return;
    RomItem *rom = &state->library.roms[state->library.selected];
    if (rom->cover || state->cover_worker.running) return;

    SceIoStat st;
    if (sceIoGetstat(rom->cover_path, &st) >= 0 && st.st_size > 0) return;

    CoverWorkerArgs *args = (CoverWorkerArgs *)malloc(sizeof(CoverWorkerArgs));
    if (!args) return;
    memset(args, 0, sizeof(*args));
    state->cover_worker.generation++;
    state->cover_worker.running = true;
    args->state = state;
    args->rom_index = state->library.selected;
    args->job.generation = state->cover_worker.generation;
    args->job.rom_id = rom->id;
    snprintf(args->job.cover_url, sizeof(args->job.cover_url), "%s", rom->cover_url);
    snprintf(args->job.cache_path, sizeof(args->job.cache_path), "%s", rom->cover_path);

    SceUID thid = sceKernelCreateThread("vita_romm_cover", cover_worker_entry, 0x10000100, 128 * 1024, 0, 0, NULL);
    if (thid < 0) {
        state->cover_worker.running = false;
        free(args);
        return;
    }
    state->cover_worker.thread = thid;
    CoverWorkerArgs *thread_args = args;
    int start = sceKernelStartThread(thid, sizeof(thread_args), &thread_args);
    if (start < 0) {
        state->cover_worker.generation++;
        state->cover_worker.running = false;
        free(args);
    }
}

void ensure_current_cover_loading(AppState *state) {
    if (state->library.count <= 0 || state->library.selected < 0 || state->library.selected >= state->library.count) return;
    RomItem *rom = &state->library.roms[state->library.selected];
    if (!rom->cover) load_current_cover_from_cache(state);
    if (!rom->cover) start_current_cover_worker(state);
}
