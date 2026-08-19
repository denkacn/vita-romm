#include "cover_cache.h"
#include "config.h"
#include "library_loader.h"
#include "net.h"
#include "romm_client.h"
#include "romm_parser.h"
#include "vita_fs.h"

static RomItem *current_rom(AppState *state) {
    if (state->library.count <= 0) return NULL;
    if (state->library.selected < 0 || state->library.selected >= state->library.count) return NULL;
    return &state->library.roms[state->library.selected];
}

static bool file_has_content(const char *path) {
    SceIoStat st;
    return sceIoGetstat(path, &st) >= 0 && st.st_size > 0;
}

static vita2d_texture *load_texture_file(const char *path) {
    vita2d_texture *texture = vita2d_load_PNG_file(path);
    if (!texture) texture = vita2d_load_JPEG_file(path);
    return texture;
}

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
    RomItem *rom = current_rom(state);
    if (!rom) return;
    if (rom->cover) return;

    if (!file_has_content(rom->cover_path)) return;
    rom->cover = load_texture_file(rom->cover_path);
    if (!rom->cover) sceIoRemove(rom->cover_path);
}

typedef struct {
    AppState *state;
    int rom_index;
    CoverJob job;
} CoverWorkerArgs;

static void finish_cover_worker(AppState *state, int generation) {
    lock_games(state);
    if (generation == state->cover_worker.generation) state->cover_worker.running = false;
    unlock_games(state);
}

static bool fetch_cover_url(AppState *state, const CoverJob *job, int rom_index, char *cover_url, size_t cover_url_size) {
    char *response = (char *)malloc(HTTP_RESPONSE_MAX + 1);
    if (!response) return false;

    char api_path[64];
    char url[360];
    long http = 0;
    snprintf(api_path, sizeof(api_path), "/api/roms/%d", job->rom_id);
    build_api_url(url, sizeof(url), state, api_path);

    CURLcode c = http_get(state, url, response, &http);
    bool found = c == CURLE_OK && http == 200 &&
        find_cover_url(response, response + strlen(response), cover_url, cover_url_size);

    if (found) {
        lock_games(state);
        if (job->generation == state->cover_worker.generation &&
            rom_index >= 0 && rom_index < state->library.count &&
            state->library.roms[rom_index].id == job->rom_id) {
            snprintf(state->library.roms[rom_index].cover_url,
                sizeof(state->library.roms[rom_index].cover_url), "%s", cover_url);
        }
        unlock_games(state);
    }

    free(response);
    return found;
}

static void download_cover(AppState *state, const CoverJob *job, const char *cover_url, const char *cover_path) {
    if (!cover_url[0] || file_has_content(cover_path)) return;

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
        return;
    }

    remove_path_recursive(part_path);
    char detail[420];
    snprintf(detail, sizeof(detail), "rom=%d http=%ld curl=%d url=%.240s", job->rom_id, http, c, url);
    log_event("cover failed", detail);
}

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

    if (!cover_url[0]) {
        fetch_cover_url(state, &job, args->rom_index, cover_url, sizeof(cover_url));
    }

    download_cover(state, &job, cover_url, cover_path);

    finish_cover_worker(state, job.generation);
    free(args);
    return 0;
}

static void start_current_cover_worker(AppState *state) {
    RomItem *rom = current_rom(state);
    if (!rom) return;
    if (rom->cover || state->cover_worker.running) return;

    if (file_has_content(rom->cover_path)) return;

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

void refresh_current_cover(AppState *state) {
    RomItem *rom = current_rom(state);
    if (!rom) return;
    if (state->cover_worker.running) {
        snprintf(state->status, sizeof(state->status), "Poster is already loading");
        return;
    }

    state->cover_worker.generation++;

    if (rom->cover) {
        vita2d_free_texture(rom->cover);
        rom->cover = NULL;
    }
    rom->cover_url[0] = '\0';
    remove_path_recursive(rom->cover_path);

    char detail[220];
    snprintf(detail, sizeof(detail), "rom=%d path=%s", rom->id, rom->cover_path);
    log_event("cover refresh requested", detail);
    snprintf(state->status, sizeof(state->status), "Refreshing poster");
    snprintf(state->detail, sizeof(state->detail), "%s", rom->name);

    start_current_cover_worker(state);
}

void ensure_current_cover_loading(AppState *state) {
    RomItem *rom = current_rom(state);
    if (!rom) return;
    if (!rom->cover) load_current_cover_from_cache(state);
    if (!rom->cover) start_current_cover_worker(state);
}
