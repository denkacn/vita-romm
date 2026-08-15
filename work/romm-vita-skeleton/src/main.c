#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/promoterutil.h>
#include <psp2/sysmodule.h>

#include <curl/curl.h>
#include <vita2d.h>
#include <zip.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "font_atlas.h"

#define APP_TITLE "Vita RomM"
#define APP_TITLEID "ROMM00001"
#ifndef ROMM_VITA_VERSION
#define ROMM_VITA_VERSION "0.2.10"
#endif
#define CONFIG_DIR "ux0:data/vita-romm"
#define CONFIG_PATH CONFIG_DIR "/config.txt"
#define POSTER_CACHE_DIR CONFIG_DIR "/covers"
#define DOWNLOAD_DIR CONFIG_DIR "/downloads"
#define FONT_RENDERER_NAME "Inter bitmap atlas"
#define HTTP_RESPONSE_MAX 131071
#define MAX_ROMS 48
#define MAX_REFRESH_TITLES 1024
#define PLATFORM_COUNT 2

#define COLOR_BG RGBA8(17, 20, 28, 255)
#define COLOR_BAR RGBA8(28, 35, 46, 255)
#define COLOR_CARD RGBA8(36, 45, 58, 255)
#define COLOR_CARD_ACTIVE RGBA8(58, 95, 132, 255)
#define COLOR_ACCENT RGBA8(91, 192, 190, 255)
#define COLOR_TEXT RGBA8(238, 242, 246, 255)
#define COLOR_MUTED RGBA8(156, 169, 184, 255)
#define COLOR_GOOD RGBA8(105, 214, 132, 255)
#define COLOR_WARN RGBA8(245, 174, 79, 255)
#define UI_FONT_SCALE 1.0f
#define UI_FONT_BASE_SIZE 32.0f
#define VITA_FILE_MODE (SCE_S_IRUSR | SCE_S_IWUSR | SCE_S_IRSYS | SCE_S_IWSYS)
#define VITA_DIR_MODE (SCE_S_IRWXU | SCE_S_IRWXS)

typedef enum {
    VIEW_LOADING,
    VIEW_PLATFORMS,
    VIEW_GAMES
} ViewMode;

typedef struct {
    const char *label;
    const char *slug;
    const char *fallback_dir;
    int id;
    int rom_count;
} PlatformDef;

typedef struct {
    int id;
    char name[160];
    char file_name[160];
    char cover_url[320];
    char cover_path[128];
    vita2d_texture *cover;
    bool is_downloaded;
    bool is_installed;
} RomItem;

typedef struct {
    char server_url[256];
    char api_token[256];
    char vita_install_dir[160];
    char vita_download_dir[160];
    char vita_app_dir[80];
    char vita_patch_dir[80];
    char vita_repatch_dir[80];
    char vita_addcont_dir[80];
    char vita_license_dir[80];
    char psp_install_dir[160];
    char psp_game_dir[160];
    char psp_download_dir[160];
    char status[256];
    char detail[256];
    char network_status[96];
    char ip_address[64];
    PlatformDef platforms[PLATFORM_COUNT];
    RomItem roms[MAX_ROMS];
    int rom_count;
    int selected_platform;
    int selected_rom;
    int progress_current;
    int progress_total;
    ViewMode view;
    bool auto_refresh_livearea;
    bool busy;
} AppState;

typedef struct {
    char root[180];
    char title_id[16];
} VitaZipHint;

typedef struct {
    char root[180];
    char folder[80];
} PspZipHint;

static char net_memory[1 * 1024 * 1024];
static char http_response[HTTP_RESPONSE_MAX + 1];
static char stats_response[HTTP_RESPONSE_MAX + 1];
typedef struct {
    vita2d_texture *atlas;
} BitmapFont;

static BitmapFont *g_font = NULL;

static void draw_ui(BitmapFont *font, const AppState *state);
static void update_rom_status(AppState *state, int index);

typedef struct {
    SceUID fd;
    AppState *state;
    curl_off_t last_drawn;
} DownloadContext;

static void trim_line(char *text) {
    size_t len = strlen(text);
    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r' || text[len - 1] == ' ' || text[len - 1] == '\t')) {
        text[len - 1] = '\0';
        len--;
    }
}

static bool starts_with(const char *text, const char *prefix) {
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

static void copy_value(char *dst, size_t dst_size, const char *value) {
    if (dst_size == 0) return;
    snprintf(dst, dst_size, "%s", value);
    trim_line(dst);
}

static void lowercase_ascii(char *text) {
    for (char *p = text; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    }
}

static bool ends_with_ci(const char *text, const char *suffix) {
    size_t text_len = strlen(text);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > text_len) return false;
    const char *a = text + text_len - suffix_len;
    for (size_t i = 0; i < suffix_len; i++) {
        char ca = a[i];
        char cb = suffix[i];
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return false;
    }
    return true;
}

static void sanitize_filename(char *text) {
    for (char *p = text; *p; p++) {
        if (*p == '/' || *p == '\\' || *p == ':' || *p == '*' || *p == '?' || *p == '"' || *p == '<' || *p == '>' || *p == '|') {
            *p = '_';
        }
    }
}

static const char *basename_ptr(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *base = slash > backslash ? slash : backslash;
    return base ? base + 1 : path;
}

static bool path_has_traversal(const char *path) {
    if (path[0] == '/' || path[0] == '\\') return true;
    if (strchr(path, ':')) return true;
    const char *p = path;
    while (*p) {
        if (p[0] == '.' && p[1] == '.' && (p[2] == '/' || p[2] == '\\' || p[2] == '\0')) return true;
        if ((p[0] == '/' || p[0] == '\\') && p[1] == '.' && p[2] == '.' && (p[3] == '/' || p[3] == '\\' || p[3] == '\0')) return true;
        p++;
    }
    return false;
}

static void mkdir_parent_for_file(const char *path) {
    char tmp[384];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp; *p; p++) {
        if (*p == '\\') *p = '/';
    }
    for (char *p = tmp; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (strlen(tmp) > 4) sceIoMkdir(tmp, VITA_DIR_MODE);
            *p = '/';
        }
    }
}

static int remove_path_recursive(const char *path) {
    SceIoStat st;
    memset(&st, 0, sizeof(st));
    if (sceIoGetstat(path, &st) < 0) return 0;

    if (SCE_S_ISDIR(st.st_mode)) {
        SceUID dfd = sceIoDopen(path);
        if (dfd >= 0) {
            while (1) {
                SceIoDirent ent;
                memset(&ent, 0, sizeof(ent));
                int res = sceIoDread(dfd, &ent);
                if (res <= 0) break;
                if (strcmp(ent.d_name, ".") == 0 || strcmp(ent.d_name, "..") == 0) continue;
                char child[384];
                snprintf(child, sizeof(child), "%s/%s", path, ent.d_name);
                remove_path_recursive(child);
            }
            sceIoDclose(dfd);
        }
        return sceIoRmdir(path);
    }

    return sceIoRemove(path);
}

static int copy_file(const char *src, const char *dst) {
    SceUID in = sceIoOpen(src, SCE_O_RDONLY, 0);
    if (in < 0) return in;
    mkdir_parent_for_file(dst);
    SceUID out = sceIoOpen(dst, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, VITA_FILE_MODE);
    if (out < 0) {
        sceIoClose(in);
        return out;
    }

    char buffer[8192];
    int result = 0;
    while (1) {
        int n = sceIoRead(in, buffer, sizeof(buffer));
        if (n < 0) {
            result = n;
            break;
        }
        if (n == 0) break;
        if (sceIoWrite(out, buffer, (SceSize)n) != n) {
            result = -1;
            break;
        }
    }

    sceIoClose(out);
    sceIoClose(in);
    return result;
}

static bool usable_file_exists(const char *path) {
    SceIoStat st;
    memset(&st, 0, sizeof(st));
    return sceIoGetstat(path, &st) >= 0 && !SCE_S_ISDIR(st.st_mode) && st.st_size > 0;
}

static void redraw_now(const AppState *state) {
    if (g_font) {
        draw_ui(g_font, state);
        sceDisplayWaitVblankStart();
    }
}

static void append_install_log(const char *line) {
    sceIoMkdir(CONFIG_DIR, VITA_DIR_MODE);
    SceUID fd = sceIoOpen(CONFIG_DIR "/install.log", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, VITA_FILE_MODE);
    if (fd < 0) return;
    sceIoWrite(fd, line, (SceSize)strlen(line));
    sceIoWrite(fd, "\n", 1);
    sceIoClose(fd);
}

static void url_encode(char *dst, size_t dst_size, const char *src) {
    static const char hex[] = "0123456789ABCDEF";
    size_t out = 0;
    for (const unsigned char *p = (const unsigned char *)src; *p && out + 4 < dst_size; p++) {
        if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' || *p == '.') {
            dst[out++] = (char)*p;
        } else {
            dst[out++] = '%';
            dst[out++] = hex[*p >> 4];
            dst[out++] = hex[*p & 15];
        }
    }
    dst[out] = '\0';
}

static void mkdirs(void) {
    sceIoMkdir(CONFIG_DIR, VITA_DIR_MODE);
    sceIoMkdir(POSTER_CACHE_DIR, VITA_DIR_MODE);
    sceIoMkdir(DOWNLOAD_DIR, VITA_DIR_MODE);
    sceIoMkdir(DOWNLOAD_DIR "/psvita", VITA_DIR_MODE);
    sceIoMkdir(DOWNLOAD_DIR "/psp", VITA_DIR_MODE);
    sceIoMkdir(CONFIG_DIR "/roms", VITA_DIR_MODE);
    sceIoMkdir(CONFIG_DIR "/roms/psvita", VITA_DIR_MODE);
    sceIoMkdir(CONFIG_DIR "/roms/psp", VITA_DIR_MODE);
}

static void init_defaults(AppState *state) {
    memset(state, 0, sizeof(*state));
    state->view = VIEW_LOADING;
    state->auto_refresh_livearea = false;
    snprintf(state->server_url, sizeof(state->server_url), "http://example.com/");
    snprintf(state->vita_install_dir, sizeof(state->vita_install_dir), CONFIG_DIR "/downloads/psvita");
    snprintf(state->vita_download_dir, sizeof(state->vita_download_dir), CONFIG_DIR "/downloads/psvita");
    snprintf(state->vita_app_dir, sizeof(state->vita_app_dir), "ux0:app");
    snprintf(state->vita_patch_dir, sizeof(state->vita_patch_dir), "ux0:patch");
    snprintf(state->vita_repatch_dir, sizeof(state->vita_repatch_dir), "ux0:rePatch");
    snprintf(state->vita_addcont_dir, sizeof(state->vita_addcont_dir), "ux0:addcont");
    snprintf(state->vita_license_dir, sizeof(state->vita_license_dir), "ux0:license");
    snprintf(state->psp_install_dir, sizeof(state->psp_install_dir), "ux0:pspemu/ISO");
    snprintf(state->psp_game_dir, sizeof(state->psp_game_dir), "ux0:pspemu/PSP/GAME");
    snprintf(state->psp_download_dir, sizeof(state->psp_download_dir), CONFIG_DIR "/downloads/psp");
    state->platforms[0] = (PlatformDef){ "PS Vita", "psvita", CONFIG_DIR "/roms/psvita", -1, 0 };
    state->platforms[1] = (PlatformDef){ "PSP", "psp", "ux0:pspemu/ISO", -1, 0 };
    snprintf(state->status, sizeof(state->status), "Starting");
    snprintf(state->detail, sizeof(state->detail), "Preparing Vita RomM");
}

static void ensure_config(AppState *state) {
    mkdirs();
    SceUID fd = sceIoOpen(CONFIG_PATH, SCE_O_RDONLY, 0);
    if (fd >= 0) {
        char data[1400];
        int n = sceIoRead(fd, data, sizeof(data) - 1);
        sceIoClose(fd);
        if (n > 0) {
            data[n] = '\0';
            char *line = strtok(data, "\n");
            while (line) {
                trim_line(line);
                if (starts_with(line, "server_url=")) copy_value(state->server_url, sizeof(state->server_url), line + 11);
                else if (starts_with(line, "api_token=")) copy_value(state->api_token, sizeof(state->api_token), line + 10);
                else if (starts_with(line, "psvita_install_dir=")) copy_value(state->vita_install_dir, sizeof(state->vita_install_dir), line + 19);
                else if (starts_with(line, "psvita_download_dir=")) copy_value(state->vita_download_dir, sizeof(state->vita_download_dir), line + 20);
                else if (starts_with(line, "psvita_app_dir=")) copy_value(state->vita_app_dir, sizeof(state->vita_app_dir), line + 15);
                else if (starts_with(line, "psvita_patch_dir=")) copy_value(state->vita_patch_dir, sizeof(state->vita_patch_dir), line + 17);
                else if (starts_with(line, "psvita_repatch_dir=")) copy_value(state->vita_repatch_dir, sizeof(state->vita_repatch_dir), line + 19);
                else if (starts_with(line, "psvita_addcont_dir=")) copy_value(state->vita_addcont_dir, sizeof(state->vita_addcont_dir), line + 19);
                else if (starts_with(line, "psvita_license_dir=")) copy_value(state->vita_license_dir, sizeof(state->vita_license_dir), line + 19);
                else if (starts_with(line, "psp_install_dir=")) copy_value(state->psp_install_dir, sizeof(state->psp_install_dir), line + 16);
                else if (starts_with(line, "psp_game_dir=")) copy_value(state->psp_game_dir, sizeof(state->psp_game_dir), line + 13);
                else if (starts_with(line, "psp_download_dir=")) copy_value(state->psp_download_dir, sizeof(state->psp_download_dir), line + 17);
                else if (starts_with(line, "auto_refresh_livearea=")) state->auto_refresh_livearea = atoi(line + 22) != 0;
                line = strtok(NULL, "\n");
            }
        }
        if (!state->vita_download_dir[0]) snprintf(state->vita_download_dir, sizeof(state->vita_download_dir), "%s", state->vita_install_dir);
        if (!state->vita_app_dir[0]) snprintf(state->vita_app_dir, sizeof(state->vita_app_dir), "ux0:app");
        if (!state->vita_patch_dir[0]) snprintf(state->vita_patch_dir, sizeof(state->vita_patch_dir), "ux0:patch");
        if (!state->vita_repatch_dir[0]) snprintf(state->vita_repatch_dir, sizeof(state->vita_repatch_dir), "ux0:rePatch");
        if (!state->vita_addcont_dir[0]) snprintf(state->vita_addcont_dir, sizeof(state->vita_addcont_dir), "ux0:addcont");
        if (!state->vita_license_dir[0]) snprintf(state->vita_license_dir, sizeof(state->vita_license_dir), "ux0:license");
        if (!state->psp_install_dir[0]) snprintf(state->psp_install_dir, sizeof(state->psp_install_dir), "ux0:pspemu/ISO");
        if (!state->psp_game_dir[0]) snprintf(state->psp_game_dir, sizeof(state->psp_game_dir), "ux0:pspemu/PSP/GAME");
        if (!state->psp_download_dir[0]) snprintf(state->psp_download_dir, sizeof(state->psp_download_dir), CONFIG_DIR "/downloads/psp");
        return;
    }

    fd = sceIoOpen(CONFIG_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, VITA_FILE_MODE);
    if (fd >= 0) {
        char cfg[700];
        snprintf(cfg, sizeof(cfg),
            "server_url=http://example.com/\n"
            "api_token=\n"
            "psvita_download_dir=%s\n"
            "psvita_app_dir=%s\n"
            "psvita_patch_dir=%s\n"
            "psvita_repatch_dir=%s\n"
            "psvita_addcont_dir=%s\n"
            "psvita_license_dir=%s\n"
            "psp_install_dir=%s\n"
            "psp_game_dir=%s\n"
            "psp_download_dir=%s\n"
            "auto_refresh_livearea=0\n",
            state->vita_download_dir,
            state->vita_app_dir,
            state->vita_patch_dir,
            state->vita_repatch_dir,
            state->vita_addcont_dir,
            state->vita_license_dir,
            state->psp_install_dir,
            state->psp_game_dir,
            state->psp_download_dir);
        sceIoWrite(fd, cfg, (SceSize)strlen(cfg));
        sceIoClose(fd);
    }
}

static void build_api_url(char *dst, size_t dst_size, const AppState *state, const char *path) {
    char base[256];
    snprintf(base, sizeof(base), "%s", state->server_url);
    trim_line(base);
    size_t len = strlen(base);
    while (len > 0 && base[len - 1] == '/') base[--len] = '\0';
    snprintf(dst, dst_size, "%s%s", base, path);
}

static void escape_url_spaces(char *url, size_t url_size) {
    char tmp[460];
    size_t out = 0;
    for (size_t i = 0; url[i] && out + 1 < sizeof(tmp); i++) {
        if (url[i] == ' ') {
            if (out + 3 >= sizeof(tmp)) break;
            tmp[out++] = '%';
            tmp[out++] = '2';
            tmp[out++] = '0';
        } else {
            tmp[out++] = url[i];
        }
    }
    tmp[out] = '\0';
    snprintf(url, url_size, "%s", tmp);
}

static void init_network(AppState *state) {
    SceNetInitParam p;
    memset(&p, 0, sizeof(p));
    p.memory = net_memory;
    p.size = sizeof(net_memory);
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceNetInit(&p);
    sceNetCtlInit();
    snprintf(state->network_status, sizeof(state->network_status), "Network initialized");
}

static void refresh_network(AppState *state) {
    int s = 0;
    if (sceNetCtlInetGetState(&s) < 0 || s != SCE_NETCTL_STATE_CONNECTED) {
        snprintf(state->network_status, sizeof(state->network_status), "Not connected");
        state->ip_address[0] = '\0';
        return;
    }
    SceNetCtlInfo info;
    memset(&info, 0, sizeof(info));
    if (sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS, &info) >= 0) {
        snprintf(state->ip_address, sizeof(state->ip_address), "%s", info.ip_address);
    }
    snprintf(state->network_status, sizeof(state->network_status), "Connected");
}

static size_t write_string_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    char *buffer = (char *)userdata;
    size_t cur = strlen(buffer);
    size_t avail = HTTP_RESPONSE_MAX - cur;
    size_t n = total < avail ? total : avail;
    if (n > 0) {
        memcpy(buffer + cur, ptr, n);
        buffer[cur + n] = '\0';
    }
    return total;
}

static size_t write_file_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    DownloadContext *ctx = (DownloadContext *)userdata;
    size_t total = size * nmemb;
    sceIoWrite(ctx->fd, ptr, (SceSize)total);
    return total;
}

static int download_progress_cb(void *userdata, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    (void)ultotal;
    (void)ulnow;

    DownloadContext *ctx = (DownloadContext *)userdata;
    if (!ctx || !ctx->state) return 0;

    if (dlnow < ctx->last_drawn + (256 * 1024) && !(dltotal > 0 && dlnow >= dltotal)) return 0;
    ctx->last_drawn = dlnow;

    double mb_now = (double)dlnow / (1024.0 * 1024.0);
    if (dltotal > 0) {
        double mb_total = (double)dltotal / (1024.0 * 1024.0);
        int percent = (int)((dlnow * 100) / dltotal);
        snprintf(ctx->state->status, sizeof(ctx->state->status), "Downloading %d%%", percent);
        snprintf(ctx->state->detail, sizeof(ctx->state->detail), "%.1f / %.1f MB", mb_now, mb_total);
    } else {
        snprintf(ctx->state->status, sizeof(ctx->state->status), "Downloading");
        snprintf(ctx->state->detail, sizeof(ctx->state->detail), "%.1f MB", mb_now);
    }

    if (g_font) {
        draw_ui(g_font, ctx->state);
        sceDisplayWaitVblankStart();
    }
    return 0;
}

static struct curl_slist *setup_headers(CURL *curl, const AppState *state, const char *url) {
    struct curl_slist *headers = NULL;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, APP_TITLE "/" ROMM_VITA_VERSION);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    if (state->api_token[0]) {
        char auth[320];
        snprintf(auth, sizeof(auth), "Authorization: Bearer %s", state->api_token);
        headers = curl_slist_append(headers, auth);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    return headers;
}

static CURLcode http_get(AppState *state, const char *url, char *response, long *code_out) {
    response[0] = '\0';
    *code_out = 0;
    CURL *curl = curl_easy_init();
    if (!curl) return CURLE_FAILED_INIT;
    struct curl_slist *headers = setup_headers(curl, state, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_string_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    CURLcode code = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, code_out);
    curl_easy_cleanup(curl);
    if (headers) curl_slist_free_all(headers);
    return code;
}

static CURLcode http_download(AppState *state, const char *url, const char *path, long *code_out) {
    *code_out = 0;
    SceUID fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, VITA_FILE_MODE);
    if (fd < 0) return CURLE_WRITE_ERROR;
    CURL *curl = curl_easy_init();
    if (!curl) {
        sceIoClose(fd);
        return CURLE_FAILED_INIT;
    }
    DownloadContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.fd = fd;
    ctx.state = state;

    struct curl_slist *headers = setup_headers(curl, state, url);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 256L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 45L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, download_progress_cb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
    CURLcode code = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, code_out);
    curl_easy_cleanup(curl);
    if (headers) curl_slist_free_all(headers);
    sceIoClose(fd);
    return code;
}

static const char *find_json_object_end(const char *start) {
    int depth = 0;
    bool in_string = false;
    bool esc = false;
    for (const char *p = start; *p; p++) {
        char c = *p;
        if (in_string) {
            if (esc) esc = false;
            else if (c == '\\') esc = true;
            else if (c == '"') in_string = false;
        } else {
            if (c == '"') in_string = true;
            else if (c == '{') depth++;
            else if (c == '}' && --depth == 0) return p;
        }
    }
    return NULL;
}

static const char *find_json_array_end(const char *start) {
    int depth = 0;
    bool in_string = false;
    bool esc = false;
    for (const char *p = start; *p; p++) {
        char c = *p;
        if (in_string) {
            if (esc) esc = false;
            else if (c == '\\') esc = true;
            else if (c == '"') in_string = false;
        } else {
            if (c == '"') in_string = true;
            else if (c == '[') depth++;
            else if (c == ']' && --depth == 0) return p;
        }
    }
    return NULL;
}

static bool find_json_array_field(const char *json, const char *field, const char **array_start, const char **array_end) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", field);
    const char *key = strstr(json, pat);
    if (!key) return false;
    const char *colon = strchr(key + strlen(pat), ':');
    if (!colon) return false;
    const char *open = strchr(colon, '[');
    if (!open) return false;
    const char *end = find_json_array_end(open);
    if (!end) return false;
    *array_start = open;
    *array_end = end;
    return true;
}

static bool extract_string(const char *start, const char *end, const char *field, char *dst, size_t dst_size) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", field);
    const char *key = strstr(start, pat);
    if (!key || key >= end) return false;
    const char *colon = strchr(key + strlen(pat), ':');
    if (!colon || colon >= end) return false;
    const char *q = strchr(colon, '"');
    if (!q || q >= end) return false;
    q++;
    const char *e = q;
    while (e < end && *e != '"') e++;
    if (e >= end) return false;
    size_t n = (size_t)(e - q);
    if (n >= dst_size) n = dst_size - 1;
    memcpy(dst, q, n);
    dst[n] = '\0';
    return true;
}

static bool extract_first_string(const char *start, const char *end, const char **fields, int field_count, char *dst, size_t dst_size) {
    for (int i = 0; i < field_count; i++) {
        if (extract_string(start, end, fields[i], dst, dst_size) && dst[0]) return true;
    }
    return false;
}

static bool extract_int(const char *start, const char *end, const char *field, int *value) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", field);
    const char *key = strstr(start, pat);
    if (!key || key >= end) return false;
    const char *colon = strchr(key + strlen(pat), ':');
    if (!colon || colon >= end) return false;
    *value = atoi(colon + 1);
    return true;
}

static bool object_contains(const char *start, const char *end, const char *needle) {
    size_t needle_len = strlen(needle);
    for (const char *p = start; p && p < end; p++) {
        p = strstr(p, needle);
        if (!p || p >= end) return false;
        if (p + needle_len <= end) return true;
    }
    return false;
}

static bool object_has_platform_id(const char *start, const char *end, int platform_id) {
    int id = -1;
    if (extract_int(start, end, "platform_id", &id) && id == platform_id) return true;
    if (extract_int(start, end, "platform", &id) && id == platform_id) return true;
    return false;
}

static void parse_platforms(AppState *state, const char *json) {
    const char *cur = json;
    while ((cur = strchr(cur, '{')) != NULL) {
        const char *end = find_json_object_end(cur);
        if (!end) break;
        int id = -1;
        char name[96] = "";
        char slug[96] = "";
        char folder[96] = "";
        extract_int(cur, end, "id", &id);
        extract_string(cur, end, "name", name, sizeof(name));
        extract_string(cur, end, "slug", slug, sizeof(slug));
        extract_string(cur, end, "fs_slug", folder, sizeof(folder));
        char all[260];
        snprintf(all, sizeof(all), "%s %s %s", name, slug, folder);
        lowercase_ascii(all);
        if (strstr(all, "psvita") || strstr(all, "vita")) state->platforms[0].id = id;
        if (strstr(all, "psp") || strstr(all, "portable")) state->platforms[1].id = id;
        cur = end + 1;
    }
}

static int extract_collection_count(const char *json) {
    int value = 0;
    const char *end = json + strlen(json);
    if (extract_int(json, end, "total", &value) && value >= 0) return value;
    if (extract_int(json, end, "count", &value) && value >= 0) return value;
    if (extract_int(json, end, "total_count", &value) && value >= 0) return value;

    int objects = 0;
    const char *tmp = json;
    while ((tmp = strstr(tmp, "\"id\"")) != NULL) {
        objects++;
        tmp += 4;
    }
    return objects;
}

static int extract_stats_count_for_platform(const AppState *state, int platform_index, const char *json) {
    const PlatformDef *platform = &state->platforms[platform_index];
    const char *cur = json;
    while ((cur = strchr(cur, '{')) != NULL) {
        const char *end = find_json_object_end(cur);
        if (!end) break;

        int count = -1;
        char name[128] = "";
        char slug[96] = "";
        extract_string(cur, end, "platform_name", name, sizeof(name));
        if (!name[0]) extract_string(cur, end, "name", name, sizeof(name));
        extract_string(cur, end, "platform_slug", slug, sizeof(slug));
        if (!slug[0]) extract_string(cur, end, "slug", slug, sizeof(slug));

        char all[260];
        snprintf(all, sizeof(all), "%s %s", name, slug);
        lowercase_ascii(all);

        char label[64];
        snprintf(label, sizeof(label), "%s", platform->label);
        lowercase_ascii(label);

        if ((strstr(all, platform->slug) || strstr(all, label)) &&
            (extract_int(cur, end, "game_count", &count) || extract_int(cur, end, "rom_count", &count) || extract_int(cur, end, "count", &count)) &&
            count >= 0) {
            return count;
        }
        cur = end + 1;
    }
    return -1;
}

static bool find_cover_url(const char *start, const char *end, char *dst, size_t dst_size) {
    const char *fields[] = { "path_cover_large", "path_cover_small", "url_cover", "cover_url", "image_url", "thumbnail_url", "thumbnail", "cover", "image", "path_cover_l", "path_cover_s", "path_cover" };
    for (int i = 0; i < 12; i++) {
        if (extract_string(start, end, fields[i], dst, dst_size) && dst[0]) return true;
    }
    return false;
}

static void parse_roms(AppState *state, int platform_index, const char *json) {
    state->rom_count = 0;
    int platform_id = state->platforms[platform_index].id;
    const char *scan_start = json;
    const char *scan_end = json + strlen(json);
    const char *array_start = NULL;
    const char *array_end = NULL;
    if (find_json_array_field(json, "items", &array_start, &array_end) ||
        find_json_array_field(json, "results", &array_start, &array_end) ||
        find_json_array_field(json, "data", &array_start, &array_end)) {
        scan_start = array_start;
        scan_end = array_end;
    } else {
        const char *first_array = strchr(json, '[');
        if (first_array) {
            const char *first_array_end = find_json_array_end(first_array);
            if (first_array_end) {
                scan_start = first_array;
                scan_end = first_array_end;
            }
        }
    }

    const char *cur = scan_start;
    while (state->rom_count < MAX_ROMS && cur < scan_end && (cur = strchr(cur, '{')) != NULL && cur < scan_end) {
        const char *end = find_json_object_end(cur);
        if (!end || end > scan_end) break;
        RomItem *rom = &state->roms[state->rom_count];
        memset(rom, 0, sizeof(*rom));
        int id = -1;
        extract_int(cur, end, "id", &id);
        if (platform_id >= 0 && object_contains(cur, end, "\"platform") && !object_has_platform_id(cur, end, platform_id)) {
            cur = end + 1;
            continue;
        }
        const char *name_fields[] = { "name", "fs_name", "file_name", "filename", "title" };
        if (id < 0 || !extract_first_string(cur, end, name_fields, 5, rom->name, sizeof(rom->name))) {
            cur = end + 1;
            continue;
        }
        rom->id = id;
        const char *file_fields[] = { "file_name", "fs_name", "filename", "path", "name" };
        if (!extract_first_string(cur, end, file_fields, 5, rom->file_name, sizeof(rom->file_name))) {
            char safe_name[160];
            snprintf(safe_name, sizeof(safe_name), "%.150s", rom->name);
            rom->file_name[0] = '\0';
            strncat(rom->file_name, safe_name, sizeof(rom->file_name) - 1);
            strncat(rom->file_name, ".zip", sizeof(rom->file_name) - strlen(rom->file_name) - 1);
            sanitize_filename(rom->file_name);
        }
        find_cover_url(cur, end, rom->cover_url, sizeof(rom->cover_url));
        snprintf(rom->cover_path, sizeof(rom->cover_path), "%s/%s_%d.png", POSTER_CACHE_DIR, state->platforms[platform_index].slug, rom->id);
        state->rom_count++;
        cur = end + 1;
    }
    state->selected_rom = 0;
}

static CURLcode get_platform_roms(AppState *state, int platform_id, int limit, long *http_out) {
    char url[380];
    char path[128];

    snprintf(path, sizeof(path), "/api/roms?platform_ids=%d&limit=%d&offset=0", platform_id, limit);
    build_api_url(url, sizeof(url), state, path);
    CURLcode c = http_get(state, url, http_response, http_out);
    if (c == CURLE_OK && *http_out == 200) return c;

    snprintf(path, sizeof(path), "/api/roms?platform_id=%d&limit=%d&offset=0", platform_id, limit);
    build_api_url(url, sizeof(url), state, path);
    return http_get(state, url, http_response, http_out);
}

static void make_absolute_url(char *dst, size_t dst_size, const AppState *state, const char *maybe_path) {
    if (starts_with(maybe_path, "http://") || starts_with(maybe_path, "https://")) {
        snprintf(dst, dst_size, "%s", maybe_path);
    } else if (starts_with(maybe_path, "/")) {
        build_api_url(dst, dst_size, state, maybe_path);
    } else {
        char path[340];
        snprintf(path, sizeof(path), "/%s", maybe_path);
        build_api_url(dst, dst_size, state, path);
    }
    escape_url_spaces(dst, dst_size);
}

static bool load_cover_url_from_rom_detail(AppState *state, RomItem *rom) {
    char api_path[64];
    char url[360];
    long http = 0;

    snprintf(api_path, sizeof(api_path), "/api/roms/%d", rom->id);
    build_api_url(url, sizeof(url), state, api_path);
    CURLcode c = http_get(state, url, http_response, &http);
    if (c != CURLE_OK || http != 200) {
        char log_line[220];
        snprintf(log_line, sizeof(log_line), "Cover detail failed: rom=%d http=%ld curl=%d", rom->id, http, c);
        append_install_log(log_line);
        return false;
    }

    if (find_cover_url(http_response, http_response + strlen(http_response), rom->cover_url, sizeof(rom->cover_url))) {
        char log_line[320];
        snprintf(log_line, sizeof(log_line), "Cover detail found: rom=%d url=%.230s", rom->id, rom->cover_url);
        append_install_log(log_line);
        return true;
    }

    char log_line[180];
    snprintf(log_line, sizeof(log_line), "Cover detail missing: rom=%d name=%.100s", rom->id, rom->name);
    append_install_log(log_line);
    return false;
}

static void load_current_cover(AppState *state) {
    if (state->rom_count <= 0) return;
    update_rom_status(state, state->selected_rom);
    RomItem *rom = &state->roms[state->selected_rom];
    if (rom->cover) return;

    SceIoStat st;
    bool need_download = sceIoGetstat(rom->cover_path, &st) < 0 || st.st_size <= 0;
    if (!need_download) {
        rom->cover = vita2d_load_PNG_file(rom->cover_path);
        if (!rom->cover) rom->cover = vita2d_load_JPEG_file(rom->cover_path);
        if (rom->cover) return;
        sceIoRemove(rom->cover_path);
        need_download = true;
    }

    if (need_download) {
        char url[400];
        char api_path[96];
        const char *fallbacks[] = {
            "/api/roms/%d/assets/cover",
            "/api/roms/%d/cover",
            "/api/assets/roms/%d/cover",
            "/api/roms/%d/assets/cover_l"
        };
        int attempts = 0;
        bool downloaded = false;

        if (!rom->cover_url[0]) {
            load_cover_url_from_rom_detail(state, rom);
        }

        if (rom->cover_url[0]) {
            long status = 0;
            make_absolute_url(url, sizeof(url), state, rom->cover_url);
            CURLcode c = http_download(state, url, rom->cover_path, &status);
            attempts++;
            if (c == CURLE_OK && status == 200) {
                downloaded = true;
            } else {
                sceIoRemove(rom->cover_path);
                char log_line[520];
                snprintf(log_line, sizeof(log_line), "Cover failed: rom=%d try=%d http=%ld curl=%d url=%.320s", rom->id, attempts, status, c, url);
                append_install_log(log_line);
            }
        } else {
            char log_line[220];
            snprintf(log_line, sizeof(log_line), "Cover missing in RomM API: rom=%d name=%.120s", rom->id, rom->name);
            append_install_log(log_line);
        }

        for (int i = 0; rom->cover_url[0] && !downloaded && i < 4; i++) {
            long status = 0;
            snprintf(api_path, sizeof(api_path), fallbacks[i], rom->id);
            build_api_url(url, sizeof(url), state, api_path);
            CURLcode c = http_download(state, url, rom->cover_path, &status);
            attempts++;
            if (c == CURLE_OK && status == 200) {
                downloaded = true;
            } else {
                sceIoRemove(rom->cover_path);
                char log_line[520];
                snprintf(log_line, sizeof(log_line), "Cover failed: rom=%d try=%d http=%ld curl=%d url=%.320s", rom->id, attempts, status, c, url);
                append_install_log(log_line);
            }
        }
    }

    rom->cover = vita2d_load_PNG_file(rom->cover_path);
    if (!rom->cover) rom->cover = vita2d_load_JPEG_file(rom->cover_path);
    if (!rom->cover) {
        char log_line[220];
        snprintf(log_line, sizeof(log_line), "Cover decode failed: rom=%d path=%s source=%.80s", rom->id, rom->cover_path, rom->cover_url);
        append_install_log(log_line);
        sceIoRemove(rom->cover_path);
    }
}

static void load_platforms_and_roms(AppState *state) {
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
    parse_platforms(state, http_response);

    build_api_url(url, sizeof(url), state, "/api/stats?include_platform_stats=true");
    c = http_get(state, url, stats_response, &http);
    bool stats_ok = c == CURLE_OK && http == 200;

    for (int p = 0; p < PLATFORM_COUNT; p++) {
        state->platforms[p].rom_count = 0;
        if (state->platforms[p].id < 0) continue;
        snprintf(state->status, sizeof(state->status), "Loading %s", state->platforms[p].label);

        if (stats_ok) {
            int stats_count = extract_stats_count_for_platform(state, p, stats_response);
            if (stats_count >= 0) {
                state->platforms[p].rom_count = stats_count;
                continue;
            }
        }

        c = get_platform_roms(state, state->platforms[p].id, 1, &http);
        if (c == CURLE_OK && http == 200) {
            state->platforms[p].rom_count = extract_collection_count(http_response);
        }
    }

    snprintf(state->status, sizeof(state->status), "Ready");
    snprintf(state->detail, sizeof(state->detail), "Choose platform");
    state->view = VIEW_PLATFORMS;
    state->busy = false;
}

static void load_games(AppState *state, int platform_index) {
    PlatformDef *p = &state->platforms[platform_index];
    if (p->id < 0) {
        snprintf(state->status, sizeof(state->status), "%s not found", p->label);
        return;
    }
    state->busy = true;
    snprintf(state->status, sizeof(state->status), "Loading %s games", p->label);
    long http = 0;
    CURLcode c = get_platform_roms(state, p->id, MAX_ROMS, &http);
    if (c != CURLE_OK || http != 200) {
        snprintf(state->status, sizeof(state->status), "ROM list error");
        snprintf(state->detail, sizeof(state->detail), "%s / HTTP %ld", curl_easy_strerror(c), http);
        state->busy = false;
        return;
    }
    parse_roms(state, platform_index, http_response);
    snprintf(state->status, sizeof(state->status), "%s games: %d", p->label, state->rom_count);
    snprintf(state->detail, sizeof(state->detail), "RomM platform id: %d. Left/Right browse, X download, O back", p->id);
    state->view = VIEW_GAMES;
    state->busy = false;
    load_current_cover(state);
}

static bool zip_entry_payload(const char *entry, const char *marker, const char **payload) {
    size_t marker_len = strlen(marker);
    if (starts_with(entry, marker)) {
        *payload = entry + marker_len;
        return **payload != '\0';
    }

    char wrapped[40];
    snprintf(wrapped, sizeof(wrapped), "/%s", marker);
    const char *p = strstr(entry, wrapped);
    if (!p) return false;
    *payload = p + strlen(wrapped);
    return **payload != '\0';
}

static bool parse_sfo_title_id(const unsigned char *data, size_t size, char *dst, size_t dst_size) {
    if (size < 20 || data[0] != 0x00 || data[1] != 'P' || data[2] != 'S' || data[3] != 'F') return false;
    unsigned int key_ofs = data[8] | (data[9] << 8) | (data[10] << 16) | (data[11] << 24);
    unsigned int data_ofs = data[12] | (data[13] << 8) | (data[14] << 16) | (data[15] << 24);
    unsigned int entries = data[16] | (data[17] << 8) | (data[18] << 16) | (data[19] << 24);
    if (key_ofs >= size || data_ofs >= size || entries > 128) return false;

    for (unsigned int i = 0; i < entries; i++) {
        size_t item = 20 + i * 16;
        if (item + 16 > size) return false;
        unsigned int key_rel = data[item] | (data[item + 1] << 8);
        unsigned int value_len = data[item + 4] | (data[item + 5] << 8) | (data[item + 6] << 16) | (data[item + 7] << 24);
        unsigned int value_rel = data[item + 12] | (data[item + 13] << 8) | (data[item + 14] << 16) | (data[item + 15] << 24);
        if (key_ofs + key_rel >= size || data_ofs + value_rel >= size) continue;
        const char *key = (const char *)(data + key_ofs + key_rel);
        if (strcmp(key, "TITLE_ID") != 0 && strcmp(key, "TITLEID") != 0) continue;
        size_t value_ofs = data_ofs + value_rel;
        size_t n = value_len;
        if (value_ofs + n > size) n = size - value_ofs;
        if (n >= dst_size) n = dst_size - 1;
        memcpy(dst, data + value_ofs, n);
        dst[n] = '\0';
        trim_line(dst);
        return dst[0] != '\0';
    }
    return false;
}

static int read_zip_entry(zip_t *archive, zip_uint64_t index, unsigned char *buffer, size_t buffer_size) {
    zip_stat_t st;
    zip_stat_init(&st);
    if (zip_stat_index(archive, index, 0, &st) != 0 || st.size > buffer_size) return -1;
    zip_file_t *file = zip_fopen_index(archive, index, 0);
    if (!file) return -1;
    zip_int64_t n = zip_fread(file, buffer, buffer_size);
    zip_fclose(file);
    return n < 0 ? -1 : (int)n;
}

static bool find_vita_zip_hint(zip_t *archive, VitaZipHint *hint) {
    memset(hint, 0, sizeof(*hint));
    zip_int64_t count = zip_get_num_entries(archive, 0);
    if (count < 0) return false;

    unsigned char sfo[32768];
    for (zip_uint64_t i = 0; i < (zip_uint64_t)count; i++) {
        const char *name_raw = zip_get_name(archive, i, 0);
        if (!name_raw) continue;
        char name[320];
        snprintf(name, sizeof(name), "%s", name_raw);
        for (char *p = name; *p; p++) {
            if (*p == '\\') *p = '/';
        }
        if (!ends_with_ci(name, "sce_sys/param.sfo") || path_has_traversal(name)) continue;

        int n = read_zip_entry(archive, i, sfo, sizeof(sfo));
        if (n <= 0 || !parse_sfo_title_id(sfo, (size_t)n, hint->title_id, sizeof(hint->title_id))) continue;

        char *marker = strstr(name, "sce_sys/param.sfo");
        if (!marker) continue;
        size_t root_len = (size_t)(marker - name);
        if (root_len >= sizeof(hint->root)) root_len = sizeof(hint->root) - 1;
        memcpy(hint->root, name, root_len);
        hint->root[root_len] = '\0';
        return true;
    }
    return false;
}

static bool extract_title_id_from_payload(const char *payload, char *dst, size_t dst_size) {
    const char *slash = strchr(payload, '/');
    if (!slash) return false;
    size_t n = (size_t)(slash - payload);
    if (n == 0 || n >= dst_size) return false;
    memcpy(dst, payload, n);
    dst[n] = '\0';
    return true;
}

static void make_folder_name_from_file(const char *file, char *dst, size_t dst_size) {
    snprintf(dst, dst_size, "%s", basename_ptr(file));
    char *dot = strrchr(dst, '.');
    if (dot) *dot = '\0';
    if (dst[0] == '\0' || strcmp(dst, "EBOOT") == 0 || strcmp(dst, "eboot") == 0) {
        snprintf(dst, dst_size, "PBP_GAME");
    }
    sanitize_filename(dst);
}

static void psp_folder_from_root(const char *root, char *dst, size_t dst_size) {
    char temp[180];
    snprintf(temp, sizeof(temp), "%s", root);
    trim_line(temp);
    size_t len = strlen(temp);
    while (len > 0 && temp[len - 1] == '/') temp[--len] = '\0';
    const char *folder = basename_ptr(temp);
    if (!folder || !folder[0]) folder = "PBP_GAME";
    snprintf(dst, dst_size, "%s", folder);
    sanitize_filename(dst);
}

static bool find_psp_zip_hint(zip_t *archive, PspZipHint *hint) {
    memset(hint, 0, sizeof(*hint));
    zip_int64_t count = zip_get_num_entries(archive, 0);
    if (count < 0) return false;

    for (zip_uint64_t i = 0; i < (zip_uint64_t)count; i++) {
        const char *name_raw = zip_get_name(archive, i, 0);
        if (!name_raw) continue;
        char name[320];
        snprintf(name, sizeof(name), "%s", name_raw);
        for (char *p = name; *p; p++) {
            if (*p == '\\') *p = '/';
        }
        if (path_has_traversal(name) || !ends_with_ci(name, "EBOOT.PBP")) continue;

        char *slash = strrchr(name, '/');
        if (slash) {
            size_t root_len = (size_t)(slash - name + 1);
            if (root_len >= sizeof(hint->root)) root_len = sizeof(hint->root) - 1;
            memcpy(hint->root, name, root_len);
            hint->root[root_len] = '\0';
            psp_folder_from_root(hint->root, hint->folder, sizeof(hint->folder));
        } else {
            hint->root[0] = '\0';
            snprintf(hint->folder, sizeof(hint->folder), "PBP_GAME");
        }
        return true;
    }
    return false;
}

static bool get_vita_title_id_for_entry(const char *entry_name, const VitaZipHint *hint, char *dst, size_t dst_size) {
    char entry[320];
    snprintf(entry, sizeof(entry), "%s", entry_name);
    for (char *p = entry; *p; p++) {
        if (*p == '\\') *p = '/';
    }
    if (path_has_traversal(entry)) return false;

    const char *payload = NULL;
    if (zip_entry_payload(entry, "app/", &payload)) {
        return extract_title_id_from_payload(payload, dst, dst_size);
    }
    if (hint && hint->title_id[0] && starts_with(entry, hint->root)) {
        snprintf(dst, dst_size, "%s", hint->title_id);
        return true;
    }
    return false;
}

static bool map_zip_entry_to_install_path(const AppState *state, int platform_index, const char *entry_name, const VitaZipHint *hint, const PspZipHint *psp_hint, char *dst, size_t dst_size) {
    char entry[320];
    snprintf(entry, sizeof(entry), "%s", entry_name);
    for (char *p = entry; *p; p++) {
        if (*p == '\\') *p = '/';
    }

    size_t len = strlen(entry);
    if (len == 0 || entry[len - 1] == '/' || path_has_traversal(entry)) return false;

    if (platform_index == 1) {
        if (ends_with_ci(entry, ".iso") || ends_with_ci(entry, ".cso")) {
            char file[180];
            snprintf(file, sizeof(file), "%s", basename_ptr(entry));
            sanitize_filename(file);
            snprintf(dst, dst_size, "%s/%s", state->psp_install_dir, file);
            return true;
        }
        if (psp_hint && psp_hint->folder[0]) {
            const char *payload = NULL;
            if (psp_hint->root[0]) {
                if (!starts_with(entry, psp_hint->root)) return false;
                payload = entry + strlen(psp_hint->root);
                if (*payload == '\0') return false;
            } else {
                payload = basename_ptr(entry);
            }
            snprintf(dst, dst_size, "%s/%s/%s", state->psp_game_dir, psp_hint->folder, payload);
            return true;
        }
        if (ends_with_ci(entry, ".pbp")) {
            char folder[80];
            make_folder_name_from_file(entry, folder, sizeof(folder));
            snprintf(dst, dst_size, "%s/%s/EBOOT.PBP", state->psp_game_dir, folder);
            return true;
        }
        return false;
    }

    const char *payload = NULL;
    if (zip_entry_payload(entry, "app/", &payload)) {
        snprintf(dst, dst_size, "%s/%s", state->vita_app_dir, payload);
        return true;
    }
    if (zip_entry_payload(entry, "patch/", &payload)) {
        snprintf(dst, dst_size, "%s/%s", state->vita_patch_dir, payload);
        return true;
    }
    if (zip_entry_payload(entry, "repatch/", &payload)) {
        snprintf(dst, dst_size, "%s/%s", state->vita_repatch_dir, payload);
        return true;
    }
    if (zip_entry_payload(entry, "addcont/", &payload)) {
        snprintf(dst, dst_size, "%s/%s", state->vita_addcont_dir, payload);
        return true;
    }
    if (zip_entry_payload(entry, "license/", &payload)) {
        snprintf(dst, dst_size, "%s/%s", state->vita_license_dir, payload);
        return true;
    }

    if (hint && hint->title_id[0] && starts_with(entry, hint->root)) {
        payload = entry + strlen(hint->root);
        if (*payload == '\0') return false;
        snprintf(dst, dst_size, "%s/%s/%s", state->vita_app_dir, hint->title_id, payload);
        return true;
    }

    return false;
}

static int load_sce_paf(void) {
    SceSysmoduleOpt opt;
    memset(&opt, 0, sizeof(opt));
    opt.result = &opt.flags;
    uint32_t args[] = { 0x400000, 0xEA60, 0x40000, 0, 0 };
    return sceSysmoduleLoadModuleInternalWithArg(SCE_SYSMODULE_INTERNAL_PAF, sizeof(args), args, &opt);
}

static int unload_sce_paf(void) {
    return sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_PAF);
}

static int promote_app_from_temp(const char *path) {
    int res = load_sce_paf();
    if (res < 0) {
        char log_line[160];
        snprintf(log_line, sizeof(log_line), "Promoter PAF load failed: 0x%08X path=%s", (unsigned int)res, path);
        append_install_log(log_line);
        return res;
    }

    res = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    if (res >= 0) {
        res = scePromoterUtilityInit();
        if (res >= 0) {
            res = scePromoterUtilityPromotePkgWithRif(path, 1);
            if (res < 0) {
                char log_line[180];
                snprintf(log_line, sizeof(log_line), "PromotePkgWithRif failed: 0x%08X path=%s", (unsigned int)res, path);
                append_install_log(log_line);
                res = scePromoterUtilityPromotePkg(path, 1);
                if (res < 0) {
                    snprintf(log_line, sizeof(log_line), "PromotePkg failed: 0x%08X path=%s", (unsigned int)res, path);
                    append_install_log(log_line);
                }
            }
            scePromoterUtilityExit();
        } else {
            char log_line[180];
            snprintf(log_line, sizeof(log_line), "Promoter init failed: 0x%08X path=%s", (unsigned int)res, path);
            append_install_log(log_line);
        }
        sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    } else {
        char log_line[180];
        snprintf(log_line, sizeof(log_line), "Promoter module load failed: 0x%08X path=%s", (unsigned int)res, path);
        append_install_log(log_line);
    }
    unload_sce_paf();
    return res;
}

static int copy_first_license_from_dir(const char *license_dir, const char *work_bin_path) {
    SceUID dfd = sceIoDopen(license_dir);
    if (dfd < 0) return dfd;

    int result = -1;
    while (1) {
        SceIoDirent ent;
        memset(&ent, 0, sizeof(ent));
        int res = sceIoDread(dfd, &ent);
        if (res <= 0) break;
        if (SCE_S_ISDIR(ent.d_stat.st_mode)) continue;
        if (!ends_with_ci(ent.d_name, ".rif") && !ends_with_ci(ent.d_name, ".bin")) continue;

        char rif_path[256];
        snprintf(rif_path, sizeof(rif_path), "%s/%s", license_dir, ent.d_name);
        result = copy_file(rif_path, work_bin_path);
        if (result >= 0) break;
    }

    sceIoDclose(dfd);
    return result;
}

static int copy_first_license_to_work_bin(const char *title_id, const char *work_bin_path) {
    char license_dir[192];
    snprintf(license_dir, sizeof(license_dir), "ux0:license/app/%s", title_id);
    int res = copy_first_license_from_dir(license_dir, work_bin_path);
    if (res >= 0) return res;

    snprintf(license_dir, sizeof(license_dir), "ux0:nonpdrm/license/app/%s", title_id);
    return copy_first_license_from_dir(license_dir, work_bin_path);
}

static int prepare_promoter_package(const char *app_path, const char *title_id) {
    char check_path[256];
    SceIoStat st;
    memset(&st, 0, sizeof(st));

    snprintf(check_path, sizeof(check_path), "%s/eboot.bin", app_path);
    if (sceIoGetstat(check_path, &st) < 0) return -0x101;

    snprintf(check_path, sizeof(check_path), "%s/sce_sys/param.sfo", app_path);
    if (sceIoGetstat(check_path, &st) < 0) return -0x102;

    snprintf(check_path, sizeof(check_path), "%s/sce_sys/package", app_path);
    sceIoMkdir(check_path, VITA_DIR_MODE);

    char work_bin_path[256];
    snprintf(work_bin_path, sizeof(work_bin_path), "%s/sce_sys/package/work.bin", app_path);
    if (sceIoGetstat(work_bin_path, &st) >= 0) return 0;

    int res = copy_first_license_to_work_bin(title_id, work_bin_path);
    if (res >= 0) return 0;

    char log_line[260];
    snprintf(log_line, sizeof(log_line), "No work.bin/rif for title=%s app_path=%s last=0x%08X", title_id, app_path, (unsigned int)res);
    append_install_log(log_line);
    return -0x103;
}

static int refresh_vita_livearea_for_path(const char *src, const char *temp, const char *title_id, bool needs_work_bin) {
    if (!title_id || !title_id[0]) return -1;

    sceIoMkdir("ux0:temp", VITA_DIR_MODE);
    remove_path_recursive(temp);

    int res = sceIoRename(src, temp);
    if (res < 0) return res;

    if (needs_work_bin) {
        res = prepare_promoter_package(temp, title_id);
        if (res < 0) {
            sceIoRename(temp, src);
            return res;
        }
    }

    res = promote_app_from_temp(temp);
    if (res < 0) {
        sceIoRename(temp, src);
        return res;
    }

    remove_path_recursive(temp);
    return 0;
}

static int refresh_vita_livearea_for_title(const AppState *state, const char *title_id) {
    char src[192];
    snprintf(src, sizeof(src), "%s/%s", state->vita_app_dir, title_id);
    return refresh_vita_livearea_for_path(src, "ux0:temp/app", title_id, true);
}

static int refresh_vita_patch_for_title(const AppState *state, const char *title_id) {
    char src[192];
    snprintf(src, sizeof(src), "%s/%s", state->vita_patch_dir, title_id);
    return refresh_vita_livearea_for_path(src, "ux0:temp/patch", title_id, false);
}

static void rom_download_path(const AppState *state, const RomItem *rom, int platform_index, char *path, size_t path_size) {
    char file[180];
    snprintf(file, sizeof(file), "%s", rom->file_name[0] ? rom->file_name : rom->name);
    sanitize_filename(file);
    if (platform_index == 1 && (ends_with_ci(file, ".iso") || ends_with_ci(file, ".cso"))) {
        snprintf(path, path_size, "%s/%s", state->psp_install_dir, file);
    } else if (platform_index == 1 && ends_with_ci(file, ".pbp")) {
        char folder[80];
        make_folder_name_from_file(file, folder, sizeof(folder));
        snprintf(path, path_size, "%s/%s/EBOOT.PBP", state->psp_game_dir, folder);
    } else {
        const char *download_dir = platform_index == 0 ? state->vita_download_dir : state->psp_download_dir;
        snprintf(path, path_size, "%s/%s", download_dir, file);
    }
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

static bool vita_title_id_for_rom(const AppState *state, const RomItem *rom, char *dst, size_t dst_size) {
    dst[0] = '\0';
    if (extract_title_id_from_text(rom->file_name, dst, dst_size)) return true;
    if (extract_title_id_from_text(rom->name, dst, dst_size)) return true;

    char path[340];
    rom_download_path(state, rom, 0, path, sizeof(path));
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

static bool rom_is_downloaded(const AppState *state, const RomItem *rom, int platform_index) {
    char path[340];
    rom_download_path(state, rom, platform_index, path, sizeof(path));
    return usable_file_exists(path);
}

static bool rom_is_installed(const AppState *state, const RomItem *rom, int platform_index) {
    if (platform_index == 0) {
        char title_id[16];
        if (!vita_title_id_for_rom(state, rom, title_id, sizeof(title_id))) return false;
        char path[224];
        snprintf(path, sizeof(path), "%s/%s/eboot.bin", state->vita_app_dir, title_id);
        return usable_file_exists(path);
    }

    char file[180];
    snprintf(file, sizeof(file), "%s", rom->file_name[0] ? rom->file_name : rom->name);
    sanitize_filename(file);
    if (ends_with_ci(file, ".iso") || ends_with_ci(file, ".cso")) {
        char path[340];
        snprintf(path, sizeof(path), "%s/%s", state->psp_install_dir, file);
        return usable_file_exists(path);
    }
    if (ends_with_ci(file, ".pbp")) {
        char path[340];
        char folder[80];
        make_folder_name_from_file(file, folder, sizeof(folder));
        snprintf(path, sizeof(path), "%s/%s/EBOOT.PBP", state->psp_game_dir, folder);
        return usable_file_exists(path);
    }
    if (!ends_with_ci(file, ".zip")) return false;

    char zip_path[340];
    rom_download_path(state, rom, 1, zip_path, sizeof(zip_path));
    if (!usable_file_exists(zip_path)) return false;

    int err = 0;
    zip_t *archive = zip_open(zip_path, ZIP_RDONLY, &err);
    if (!archive) return false;
    PspZipHint hint;
    bool has_hint = find_psp_zip_hint(archive, &hint);
    if (has_hint && hint.folder[0]) {
        zip_close(archive);
        char path[340];
        snprintf(path, sizeof(path), "%s/%s/EBOOT.PBP", state->psp_game_dir, hint.folder);
        return usable_file_exists(path);
    }
    zip_int64_t count = zip_get_num_entries(archive, 0);
    bool installed = false;
    for (zip_uint64_t i = 0; count > 0 && i < (zip_uint64_t)count; i++) {
        const char *name = zip_get_name(archive, i, 0);
        if (!name) continue;
        if (!ends_with_ci(name, ".iso") && !ends_with_ci(name, ".cso")) continue;
        char file[180];
        snprintf(file, sizeof(file), "%s", basename_ptr(name));
        sanitize_filename(file);
        char path[340];
        snprintf(path, sizeof(path), "%s/%s", state->psp_install_dir, file);
        if (usable_file_exists(path)) {
            installed = true;
            break;
        }
    }
    zip_close(archive);
    if (installed) return true;
    return false;
}

static void update_rom_status(AppState *state, int index) {
    if (index < 0 || index >= state->rom_count) return;
    RomItem *rom = &state->roms[index];
    rom->is_downloaded = rom_is_downloaded(state, rom, state->selected_platform);
    rom->is_installed = rom_is_installed(state, rom, state->selected_platform);
}

static bool validate_vita_install(const AppState *state, const char *title_id, char *missing, size_t missing_size) {
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

static bool is_vita_title_dir_name(const char *name) {
    if (!name || strlen(name) != 9) return false;
    for (int i = 0; i < 9; i++) {
        char c = name[i];
        bool digit = c >= '0' && c <= '9';
        bool upper = c >= 'A' && c <= 'Z';
        if (!digit && !upper) return false;
    }
    return true;
}

static bool livearea_entry_exists(const char *title_id) {
    char path[96];
    SceIoStat st;
    memset(&st, 0, sizeof(st));
    snprintf(path, sizeof(path), "ux0:appmeta/%s/param.sfo", title_id);
    if (sceIoGetstat(path, &st) >= 0 && !SCE_S_ISDIR(st.st_mode) && st.st_size > 0) return true;
    snprintf(path, sizeof(path), "ux0:appmeta/%s", title_id);
    return sceIoGetstat(path, &st) >= 0 && SCE_S_ISDIR(st.st_mode);
}

static void refresh_livearea_from_main(AppState *state) {
    char title_ids[MAX_REFRESH_TITLES][16];
    char patch_ids[MAX_REFRESH_TITLES][16];
    int title_count = 0;
    int patch_count = 0;

    state->busy = true;
    state->progress_current = 0;
    state->progress_total = 0;
    snprintf(state->status, sizeof(state->status), "Scanning Vita apps");
    snprintf(state->detail, sizeof(state->detail), "%.200s", state->vita_app_dir);
    redraw_now(state);

    SceUID dfd = sceIoDopen(state->vita_app_dir);
    if (dfd < 0) {
        snprintf(state->status, sizeof(state->status), "Refresh failed");
        snprintf(state->detail, sizeof(state->detail), "Cannot open %.180s: 0x%08X", state->vita_app_dir, (unsigned int)dfd);
        state->busy = false;
        return;
    }

    while (title_count < MAX_REFRESH_TITLES) {
        SceIoDirent ent;
        memset(&ent, 0, sizeof(ent));
        int res = sceIoDread(dfd, &ent);
        if (res <= 0) break;
        if (!SCE_S_ISDIR(ent.d_stat.st_mode)) continue;
        if (strcmp(ent.d_name, ".") == 0 || strcmp(ent.d_name, "..") == 0) continue;
        if (strcmp(ent.d_name, APP_TITLEID) == 0) continue;
        if (!is_vita_title_dir_name(ent.d_name)) continue;
        snprintf(title_ids[title_count], sizeof(title_ids[title_count]), "%s", ent.d_name);
        title_count++;
    }
    sceIoDclose(dfd);

    dfd = sceIoDopen(state->vita_patch_dir);
    if (dfd >= 0) {
        while (patch_count < MAX_REFRESH_TITLES) {
            SceIoDirent ent;
            memset(&ent, 0, sizeof(ent));
            int res = sceIoDread(dfd, &ent);
            if (res <= 0) break;
            if (!SCE_S_ISDIR(ent.d_stat.st_mode)) continue;
            if (strcmp(ent.d_name, ".") == 0 || strcmp(ent.d_name, "..") == 0) continue;
            if (!is_vita_title_dir_name(ent.d_name)) continue;
            snprintf(patch_ids[patch_count], sizeof(patch_ids[patch_count]), "%s", ent.d_name);
            patch_count++;
        }
        sceIoDclose(dfd);
    }

    if (title_count == 0 && patch_count == 0) {
        snprintf(state->status, sizeof(state->status), "Refresh LiveArea");
        snprintf(state->detail, sizeof(state->detail), "No Vita app/patch folders found");
        state->progress_current = 0;
        state->progress_total = 0;
        state->busy = false;
        return;
    }

    state->progress_total = title_count + patch_count;
    state->progress_current = 0;
    int ok = 0;
    int failed = 0;
    int last_error = 0;
    char last_failed[16] = "";
    for (int i = 0; i < title_count; i++) {
        state->progress_current = i + 1;
        snprintf(state->status, sizeof(state->status), "Refreshing LiveArea");
        snprintf(state->detail, sizeof(state->detail), "app %.9s  %d/%d  OK:%d  Fail:%d", title_ids[i], i + 1, title_count + patch_count, ok, failed);
        redraw_now(state);

        int res = 0;
        if (!livearea_entry_exists(title_ids[i])) {
            res = refresh_vita_livearea_for_title(state, title_ids[i]);
        }
        if (res >= 0) {
            ok++;
        } else {
            failed++;
            last_error = res;
            snprintf(last_failed, sizeof(last_failed), "%.9s", title_ids[i]);
            char log_line[180];
            snprintf(log_line, sizeof(log_line), "Refresh app failed: title=%s res=0x%08X", title_ids[i], (unsigned int)res);
            append_install_log(log_line);
        }
    }
    for (int i = 0; i < patch_count; i++) {
        state->progress_current = title_count + i + 1;
        snprintf(state->status, sizeof(state->status), "Refreshing LiveArea");
        snprintf(state->detail, sizeof(state->detail), "patch %.9s  %d/%d  OK:%d  Fail:%d", patch_ids[i], title_count + i + 1, title_count + patch_count, ok, failed);
        redraw_now(state);

        int res = refresh_vita_patch_for_title(state, patch_ids[i]);
        if (res >= 0) {
            ok++;
        } else {
            failed++;
            last_error = res;
            snprintf(last_failed, sizeof(last_failed), "%.9s", patch_ids[i]);
            char log_line[180];
            snprintf(log_line, sizeof(log_line), "Refresh patch failed: title=%s res=0x%08X", patch_ids[i], (unsigned int)res);
            append_install_log(log_line);
        }
    }

    if (failed == 0) {
        state->progress_current = state->progress_total;
        snprintf(state->status, sizeof(state->status), "Refresh complete");
        snprintf(state->detail, sizeof(state->detail), "LiveArea refreshed: %d / %d", ok, title_count + patch_count);
    } else {
        state->progress_current = state->progress_total;
        snprintf(state->status, sizeof(state->status), "Refresh complete with errors");
        snprintf(state->detail, sizeof(state->detail), "OK:%d Fail:%d Last:%s 0x%08X", ok, failed, last_failed, (unsigned int)last_error);
    }
    redraw_now(state);
    state->progress_current = 0;
    state->progress_total = 0;
    state->busy = false;
}

static int copy_zip_file_to_path(zip_t *archive, zip_uint64_t index, const char *dst) {
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

static int install_zip(AppState *state, const char *zip_path, int platform_index, int *matched, int *skipped, char *vita_title_id, size_t vita_title_id_size, char *first_failed, size_t first_failed_size) {
    int err = 0;
    zip_t *archive = zip_open(zip_path, ZIP_RDONLY, &err);
    if (!archive) return -1;

    int installed = 0;
    *matched = 0;
    *skipped = 0;
    if (first_failed_size > 0) first_failed[0] = '\0';
    VitaZipHint hint;
    bool has_hint = platform_index == 0 && find_vita_zip_hint(archive, &hint);
    PspZipHint psp_hint;
    bool has_psp_hint = platform_index == 1 && find_psp_zip_hint(archive, &psp_hint);
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

        if (platform_index == 0 && vita_title_id_size > 0 && vita_title_id[0] == '\0') {
            get_vita_title_id_for_entry(name, has_hint ? &hint : NULL, vita_title_id, vita_title_id_size);
        }

        char dst[384];
        if (!map_zip_entry_to_install_path(state, platform_index, name, has_hint ? &hint : NULL, has_psp_hint ? &psp_hint : NULL, dst, sizeof(dst))) {
            (*skipped)++;
            continue;
        }
        (*matched)++;

        int copy_res = copy_zip_file_to_path(archive, i, dst);
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
        char log_line[520];
        snprintf(log_line, sizeof(log_line), "ZIP copy failed: %s", first_failed);
        append_install_log(log_line);
    }
    return installed;
}

static void download_selected_rom(AppState *state) {
    if (state->rom_count <= 0) return;
    PlatformDef *p = &state->platforms[state->selected_platform];
    RomItem *rom = &state->roms[state->selected_rom];
    char encoded[240];
    char url[460];
    char path[340];
    char part_path[360];
    char file[180];
    long http = 0;

    snprintf(file, sizeof(file), "%s", rom->file_name[0] ? rom->file_name : rom->name);
    sanitize_filename(file);
    url_encode(encoded, sizeof(encoded), file);
    const char *download_dir = state->selected_platform == 0 ? state->vita_download_dir : state->psp_download_dir;
    const char *install_dir = state->selected_platform == 0 ? state->vita_download_dir : state->psp_install_dir;
    if (state->selected_platform == 1 && (ends_with_ci(file, ".iso") || ends_with_ci(file, ".cso"))) {
        snprintf(path, sizeof(path), "%s/%s", install_dir, file);
    } else if (state->selected_platform == 1 && ends_with_ci(file, ".pbp")) {
        char folder[80];
        make_folder_name_from_file(file, folder, sizeof(folder));
        snprintf(path, sizeof(path), "%s/%s/EBOOT.PBP", state->psp_game_dir, folder);
    } else {
        snprintf(path, sizeof(path), "%s/%s", download_dir, file);
    }
    mkdir_parent_for_file(path);
    snprintf(part_path, sizeof(part_path), "%s.part", path);

    char api_path[300];
    snprintf(api_path, sizeof(api_path), "/api/roms/%d/content/%s", rom->id, encoded);
    build_api_url(url, sizeof(url), state, api_path);

    state->busy = true;
    CURLcode c = CURLE_OK;
    if (usable_file_exists(path)) {
        http = 200;
        snprintf(state->status, sizeof(state->status), "Using cached file");
        snprintf(state->detail, sizeof(state->detail), "%.255s", path);
        redraw_now(state);
    } else {
        remove_path_recursive(part_path);
        snprintf(state->status, sizeof(state->status), "Downloading");
        snprintf(state->detail, sizeof(state->detail), "%s -> %s", rom->name, p->slug);
        redraw_now(state);
        c = http_download(state, url, part_path, &http);
        if (c == CURLE_OK && http == 200) {
            remove_path_recursive(path);
            int rename_res = sceIoRename(part_path, path);
            if (rename_res < 0) {
                c = CURLE_WRITE_ERROR;
            }
        }
    }
    if (c == CURLE_OK && http == 200) {
        if (ends_with_ci(path, ".zip")) {
            snprintf(state->status, sizeof(state->status), "Installing ZIP");
            snprintf(state->detail, sizeof(state->detail), "%.255s", path);
            redraw_now(state);
            int skipped = 0;
            int matched = 0;
            char vita_title_id[16];
            char first_failed[256];
            int installed = install_zip(state, path, state->selected_platform, &matched, &skipped, vita_title_id, sizeof(vita_title_id), first_failed, sizeof(first_failed));
            if (installed > 0) {
                if (state->selected_platform == 0 && vita_title_id[0]) {
                    char missing[224];
                    bool app_ok = validate_vita_install(state, vita_title_id, missing, sizeof(missing));
                    if (!app_ok) {
                        snprintf(state->status, sizeof(state->status), "Installed, app invalid");
                        if (first_failed[0]) {
                            snprintf(state->detail, sizeof(state->detail), "Files:%d/%d missing:%.90s fail:%.70s", installed, matched, missing, first_failed);
                        } else {
                            snprintf(state->detail, sizeof(state->detail), "Files:%d matched:%d missing: %.150s", installed, matched, missing);
                        }
                        char log_line[520];
                        snprintf(log_line, sizeof(log_line), "Install invalid: title=%s files=%d matched=%d skipped=%d missing=%s first_failed=%s", vita_title_id, installed, matched, skipped, missing, first_failed);
                        append_install_log(log_line);
                    } else
                    if (state->auto_refresh_livearea) {
                        snprintf(state->status, sizeof(state->status), "Refreshing LiveArea");
                        snprintf(state->detail, sizeof(state->detail), "%s", vita_title_id);
                        redraw_now(state);
                        int promote = refresh_vita_livearea_for_title(state, vita_title_id);
                        if (promote >= 0) {
                            snprintf(state->status, sizeof(state->status), "Installed");
                            snprintf(state->detail, sizeof(state->detail), "Files: %d, skipped: %d. LiveArea refreshed: %s", installed, skipped, vita_title_id);
                        } else {
                            snprintf(state->status, sizeof(state->status), "Installed, refresh failed");
                            snprintf(state->detail, sizeof(state->detail), "Files: %d, skipped: %d. Try VitaShell refresh. Promoter: 0x%08X", installed, skipped, (unsigned int)promote);
                        }
                    } else {
                        snprintf(state->status, sizeof(state->status), "Installed");
                        snprintf(state->detail, sizeof(state->detail), "App OK: %s. Press Triangle or use VitaShell Refresh LiveArea", vita_title_id);
                    }
                } else {
                    snprintf(state->status, sizeof(state->status), "Installed");
                    snprintf(state->detail, sizeof(state->detail), "Files: %d, skipped: %d", installed, skipped);
                }
            } else if (installed == 0) {
                if (matched > 0) {
                    snprintf(state->status, sizeof(state->status), "ZIP extract failed");
                    snprintf(state->detail, sizeof(state->detail), "Matched:%d skipped:%d fail:%.130s", matched, skipped, first_failed[0] ? first_failed : "Check ZIP compression/encryption");
                } else {
                    snprintf(state->status, sizeof(state->status), "Downloaded, not installed");
                    snprintf(state->detail, sizeof(state->detail), "Unknown ZIP layout. Saved: %.190s", path);
                }
            } else {
                snprintf(state->status, sizeof(state->status), "ZIP open failed");
                snprintf(state->detail, sizeof(state->detail), "Saved: %.220s", path);
            }
        } else {
            snprintf(state->status, sizeof(state->status), "%s", state->selected_platform == 1 ? "Installed" : "Downloaded");
            snprintf(state->detail, sizeof(state->detail), "%.255s", path);
        }
    } else {
        snprintf(state->status, sizeof(state->status), "Download failed");
        snprintf(state->detail, sizeof(state->detail), "%s / HTTP %ld", curl_easy_strerror(c), http);
    }
    update_rom_status(state, state->selected_rom);
    state->busy = false;
}

static int font_size_index(float scale) {
    if (scale >= 1.5f) return 5;
    if (scale >= 1.15f) return 4;
    if (scale >= 1.0f) return 3;
    if (scale >= 0.85f) return 2;
    if (scale >= 0.72f) return 1;
    return 0;
}

static void draw_text(BitmapFont *font, int x, int y, float scale, unsigned int color, const char *text) {
    if (!font || !font->atlas || !text) return;
    int size_index = font_size_index(scale);
    float pen_x = (float)x;
    int baseline_y = y;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if (*p == '\n') {
            pen_x = (float)x;
            baseline_y += FONT_ATLAS_SIZES[size_index].line_height;
            continue;
        }
        unsigned char ch = *p;
        if (ch < FONT_ATLAS_FIRST || ch >= FONT_ATLAS_FIRST + FONT_ATLAS_COUNT) ch = '?';
        const BitmapGlyph *glyph = &FONT_ATLAS_GLYPHS[size_index][ch - FONT_ATLAS_FIRST];
        if (ch != ' ') {
            vita2d_draw_texture_tint_part(font->atlas,
                pen_x + glyph->xoff,
                (float)(baseline_y + glyph->yoff),
                glyph->x,
                glyph->y,
                glyph->w,
                glyph->h,
                color);
        }
        pen_x += glyph->advance;
    }
}

static void draw_top(BitmapFont *font, const AppState *state) {
    vita2d_draw_rectangle(0, 0, 960, 74, COLOR_BAR);
    vita2d_draw_rectangle(0, 72, 960, 2, COLOR_ACCENT);
    draw_text(font, 34, 48, 1.2f, COLOR_TEXT, APP_TITLE);
    draw_text(font, 228, 47, 0.64f, COLOR_ACCENT, "v" ROMM_VITA_VERSION);
    draw_text(font, 326, 47, 0.64f, COLOR_MUTED, state->network_status);
    if (state->ip_address[0]) draw_text(font, 510, 47, 0.64f, COLOR_MUTED, state->ip_address);
}

static void draw_loading(BitmapFont *font, const AppState *state) {
    draw_text(font, 300, 230, 1.2f, COLOR_TEXT, "Loading RomM library");
    vita2d_draw_rectangle(300, 260, 360, 8, COLOR_CARD);
    vita2d_draw_rectangle(300, 260, state->busy ? 220 : 360, 8, COLOR_ACCENT);
    draw_text(font, 300, 306, 0.72f, COLOR_MUTED, state->status);
    draw_text(font, 300, 334, 0.62f, COLOR_MUTED, state->detail);
}

static void draw_platforms(BitmapFont *font, const AppState *state) {
    vita2d_draw_rectangle(54, 82, 520, 34, RGBA8(186, 85, 211, 255));
    draw_text(font, 66, 108, 0.72f, COLOR_BG, "BITMAP FONT TEST 0.2.10 Inter atlas");
    draw_text(font, 56, 138, 0.86f, COLOR_MUTED, "Choose platform");
    draw_text(font, 782, 138, 0.62f, COLOR_MUTED, "Version " ROMM_VITA_VERSION);
    for (int i = 0; i < PLATFORM_COUNT; i++) {
        int x = 88 + i * 420;
        unsigned int color = i == state->selected_platform ? COLOR_CARD_ACTIVE : COLOR_CARD;
        vita2d_draw_rectangle(x, 150, 360, 230, color);
        vita2d_draw_rectangle(x + 26, 178, 86, 86, COLOR_BG);
        draw_text(font, x + 44, 234, 1.6f, COLOR_ACCENT, i == 0 ? "V" : "P");
        draw_text(font, x + 132, 224, 1.05f, COLOR_TEXT, state->platforms[i].label);
        char count[80];
        snprintf(count, sizeof(count), "%d ROMs", state->platforms[i].rom_count);
        draw_text(font, x + 132, 262, 0.85f, COLOR_GOOD, count);
        char id[80];
        snprintf(id, sizeof(id), "RomM id: %d", state->platforms[i].id);
        draw_text(font, x + 132, 296, 0.62f, COLOR_MUTED, id);
    }
    vita2d_draw_rectangle(300, 410, 360, 46, COLOR_CARD);
    draw_text(font, 330, 441, 0.74f, COLOR_ACCENT, "Triangle");
    draw_text(font, 454, 441, 0.74f, COLOR_TEXT, "Refresh LiveArea");
    if (state->busy && state->progress_total > 0) {
        int bar_x = 160;
        int bar_y = 470;
        int bar_w = 640;
        int fill = (state->progress_current * bar_w) / state->progress_total;
        if (fill < 0) fill = 0;
        if (fill > bar_w) fill = bar_w;
        vita2d_draw_rectangle(bar_x, bar_y, bar_w, 12, COLOR_CARD);
        vita2d_draw_rectangle(bar_x, bar_y, fill, 12, COLOR_ACCENT);
        char progress[64];
        snprintf(progress, sizeof(progress), "%d / %d", state->progress_current, state->progress_total);
        draw_text(font, 820, 482, 0.58f, COLOR_MUTED, progress);
    }
    draw_text(font, 58, 500, 0.68f, COLOR_MUTED, "Left/Right select, X open games");
}

static void draw_games(BitmapFont *font, const AppState *state) {
    PlatformDef *p = (PlatformDef *)&state->platforms[state->selected_platform];
    draw_text(font, 42, 108, 0.74f, COLOR_MUTED, p->label);
    if (state->rom_count <= 0) {
        draw_text(font, 300, 270, 0.9f, COLOR_WARN, "No games loaded");
        return;
    }
    RomItem *rom = (RomItem *)&state->roms[state->selected_rom];
    vita2d_draw_rectangle(58, 138, 300, 330, COLOR_CARD);
    if (rom->cover) {
        vita2d_draw_texture_scale(rom->cover, 78, 154, 260.0f / vita2d_texture_get_width(rom->cover), 300.0f / vita2d_texture_get_height(rom->cover));
    } else {
        vita2d_draw_rectangle(92, 174, 232, 260, COLOR_BG);
        draw_text(font, 130, 314, 0.86f, COLOR_MUTED, "No poster");
    }
    draw_text(font, 400, 190, 1.0f, COLOR_TEXT, rom->name);
    char meta[120];
    snprintf(meta, sizeof(meta), "%d / %d    RomM id: %d", state->selected_rom + 1, state->rom_count, rom->id);
    draw_text(font, 400, 236, 0.72f, COLOR_MUTED, meta);
    int badge_x = 400;
    if (rom->is_downloaded) {
        vita2d_draw_rectangle(badge_x, 258, 150, 34, COLOR_CARD_ACTIVE);
        draw_text(font, badge_x + 14, 282, 0.58f, COLOR_TEXT, "Downloaded");
        badge_x += 166;
    }
    if (rom->is_installed) {
        vita2d_draw_rectangle(badge_x, 258, 122, 34, COLOR_GOOD);
        draw_text(font, badge_x + 14, 282, 0.58f, COLOR_BG, "Installed");
    }
    draw_text(font, 400, 302, 0.72f, COLOR_GOOD, "X download/install");
    draw_text(font, 400, 336, 0.72f, COLOR_MUTED, "Left/Right browse, O back");
    draw_text(font, 400, 414, 0.62f, COLOR_MUTED, state->detail);
}

static void draw_ui(BitmapFont *font, const AppState *state) {
    vita2d_start_drawing();
    vita2d_clear_screen();
    vita2d_draw_rectangle(0, 0, 960, 544, COLOR_BG);
    draw_top(font, state);
    if (state->view == VIEW_LOADING) draw_loading(font, state);
    else if (state->view == VIEW_PLATFORMS) draw_platforms(font, state);
    else draw_games(font, state);
    draw_text(font, 42, 522, 0.58f, state->busy ? COLOR_WARN : COLOR_MUTED, state->status);
    vita2d_end_drawing();
    vita2d_swap_buffers();
}

int main(void) {
    AppState state;
    init_defaults(&state);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    ensure_config(&state);
    init_network(&state);
    refresh_network(&state);
    curl_global_init(CURL_GLOBAL_DEFAULT);

    vita2d_init();
    vita2d_set_clear_color(COLOR_BG);
    BitmapFont font;
    memset(&font, 0, sizeof(font));
    font.atlas = vita2d_load_PNG_file(FONT_ATLAS_PATH);
    if (!font.atlas) {
        sceKernelExitProcess(1);
        return 1;
    }
    g_font = &font;

    draw_ui(&font, &state);
    sceDisplayWaitVblankStart();
    load_platforms_and_roms(&state);

    SceCtrlData pad;
    unsigned int old_buttons = 0;
    bool running = true;
    while (running) {
        memset(&pad, 0, sizeof(pad));
        sceCtrlPeekBufferPositive(0, &pad, 1);
        unsigned int pressed = pad.buttons & ~old_buttons;
        old_buttons = pad.buttons;

        if (!state.busy && state.view == VIEW_PLATFORMS) {
            if (pressed & (SCE_CTRL_LEFT | SCE_CTRL_RIGHT)) {
                state.selected_platform = 1 - state.selected_platform;
            }
            if (pressed & SCE_CTRL_CROSS) {
                draw_ui(&font, &state);
                load_games(&state, state.selected_platform);
            }
            if (pressed & SCE_CTRL_TRIANGLE) {
                draw_ui(&font, &state);
                refresh_livearea_from_main(&state);
            }
        } else if (!state.busy && state.view == VIEW_GAMES) {
            if ((pressed & SCE_CTRL_LEFT) && state.rom_count > 0) {
                state.selected_rom = (state.selected_rom + state.rom_count - 1) % state.rom_count;
                load_current_cover(&state);
            }
            if ((pressed & SCE_CTRL_RIGHT) && state.rom_count > 0) {
                state.selected_rom = (state.selected_rom + 1) % state.rom_count;
                load_current_cover(&state);
            }
            if (pressed & SCE_CTRL_CROSS) {
                draw_ui(&font, &state);
                download_selected_rom(&state);
            }
            if (pressed & SCE_CTRL_CIRCLE) {
                state.view = VIEW_PLATFORMS;
                snprintf(state.status, sizeof(state.status), "Ready");
                snprintf(state.detail, sizeof(state.detail), "Choose platform");
            }
        }

        draw_ui(&font, &state);
        sceDisplayWaitVblankStart();
    }

    for (int i = 0; i < state.rom_count; i++) {
        if (state.roms[i].cover) vita2d_free_texture(state.roms[i].cover);
    }
    if (font.atlas) vita2d_free_texture(font.atlas);
    g_font = NULL;
    vita2d_fini();
    curl_global_cleanup();
    sceKernelExitProcess(0);
    return 0;
}









