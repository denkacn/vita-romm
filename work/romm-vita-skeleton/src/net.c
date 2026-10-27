#include "net.h"
#include "ui.h"
#include "ui_resources.h"
#include "vita_fs.h"

static char net_memory[1 * 1024 * 1024];

enum {
    DOWNLOAD_WRITE_BUF_SIZE = 2 * 1024 * 1024,
    DOWNLOAD_CURL_BUF_SIZE = 512 * 1024
};

static unsigned long long net_now_ms(void) {
    return (unsigned long long)(sceKernelGetProcessTimeWide() / 1000ULL);
}

static const char *url_debug_path(const char *url) {
    const char *p = strstr(url, "://");
    if (!p) return url;
    p = strchr(p + 3, '/');
    return p ? p : "/";
}

static void log_http_result(const AppState *state, const char *kind, const char *url, CURLcode curl_code, long http_code, unsigned long long elapsed_ms, size_t bytes) {
    char detail[420];
    snprintf(detail, sizeof(detail), "%s curl=%d http=%ld ms=%llu bytes=%u path=%.260s",
        kind, curl_code, http_code, elapsed_ms, (unsigned int)bytes, url_debug_path(url));
    debug_log_event(state, "http", detail);
}

void init_network(AppState *state) {
    SceNetInitParam p;
    memset(&p, 0, sizeof(p));
    p.memory = net_memory;
    p.size = sizeof(net_memory);
    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
    sceNetInit(&p);
    sceNetCtlInit();
    snprintf(state->network_status, sizeof(state->network_status), "Network initialized");
    debug_log_event(state, "network init", "sceNet initialized");
}

void refresh_network(AppState *state) {
    int s = 0;
    if (sceNetCtlInetGetState(&s) < 0 || s != SCE_NETCTL_STATE_CONNECTED) {
        snprintf(state->network_status, sizeof(state->network_status), "Not connected");
        state->ip_address[0] = '\0';
        debug_log_event(state, "network state", "not connected");
        return;
    }
    SceNetCtlInfo info;
    memset(&info, 0, sizeof(info));
    if (sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS, &info) >= 0) {
        snprintf(state->ip_address, sizeof(state->ip_address), "%s", info.ip_address);
    }
    snprintf(state->network_status, sizeof(state->network_status), "Connected");
    char detail[120];
    snprintf(detail, sizeof(detail), "connected ip=%s", state->ip_address);
    debug_log_event(state, "network state", detail);
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

static bool write_all(DownloadContext *ctx, const char *data, size_t len) {
    while (len > 0) {
        int n = sceIoWrite(ctx->fd, data, (SceSize)len);
        if (n <= 0) {
            ctx->write_failed = true;
            return false;
        }
        data += n;
        len -= (size_t)n;
    }
    return true;
}

static bool flush_write_buf(DownloadContext *ctx) {
    if (!ctx->write_buf || ctx->write_len == 0) return !ctx->write_failed;
    bool ok = write_all(ctx, ctx->write_buf, ctx->write_len);
    ctx->write_len = 0;
    return ok;
}

// Memory card writes are slow in small chunks, so batch curl's ~16 KB
// callbacks into one large sceIoWrite.
static size_t write_file_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    DownloadContext *ctx = (DownloadContext *)userdata;
    size_t total = size * nmemb;
    if (!ctx->write_buf) return write_all(ctx, ptr, total) ? total : 0;
    if (ctx->write_len + total > ctx->write_cap && !flush_write_buf(ctx)) return 0;
    if (total >= ctx->write_cap) return write_all(ctx, ptr, total) ? total : 0;
    memcpy(ctx->write_buf + ctx->write_len, ptr, total);
    ctx->write_len += total;
    return total;
}

static int download_progress_cb(void *userdata, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    (void)ultotal;
    (void)ulnow;

    DownloadContext *ctx = (DownloadContext *)userdata;
    if (!ctx || !ctx->state) return 0;

    if (dlnow < ctx->last_drawn + (256 * 1024) && !(dltotal > 0 && dlnow >= dltotal)) return 0;
    ctx->last_drawn = dlnow;
    // No buttons are pressed during a long download, so keep the system from
    // dimming and auto-suspending, which drops Wi-Fi and kills the transfer.
    sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);

    double mb_now = (double)dlnow / (1024.0 * 1024.0);
    if (dltotal > 0) {
        double mb_total = (double)dltotal / (1024.0 * 1024.0);
        int percent = (int)((dlnow * 100) / dltotal);
        if (ctx->state->progress.total > 1) {
            snprintf(ctx->state->status, sizeof(ctx->state->status), "Downloading file %d/%d %d%%", ctx->state->progress.current, ctx->state->progress.total, percent);
        } else {
            snprintf(ctx->state->status, sizeof(ctx->state->status), "Downloading %d%%", percent);
        }
        snprintf(ctx->state->detail, sizeof(ctx->state->detail), "%.1f / %.1f MB", mb_now, mb_total);
    } else {
        if (ctx->state->progress.total > 1) {
            snprintf(ctx->state->status, sizeof(ctx->state->status), "Downloading file %d/%d", ctx->state->progress.current, ctx->state->progress.total);
        } else {
            snprintf(ctx->state->status, sizeof(ctx->state->status), "Downloading");
        }
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
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 18L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 300L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
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

CURLcode http_get(AppState *state, const char *url, char *response, long *code_out) {
    response[0] = '\0';
    *code_out = 0;
    CURL *curl = curl_easy_init();
    if (!curl) return CURLE_FAILED_INIT;
    struct curl_slist *headers = setup_headers(curl, state, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_string_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    unsigned long long t0 = net_now_ms();
    CURLcode code = curl_easy_perform(curl);
    unsigned long long elapsed = net_now_ms() - t0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, code_out);
    curl_easy_cleanup(curl);
    if (headers) curl_slist_free_all(headers);
    log_http_result(state, "get", url, code, *code_out, elapsed, strlen(response));
    return code;
}

CURLcode http_get_reuse(AppState *state, CURL *curl, const char *url, char *response, long *code_out) {
    if (!curl) return http_get(state, url, response, code_out);
    response[0] = '\0';
    *code_out = 0;
    curl_easy_reset(curl);
    struct curl_slist *headers = setup_headers(curl, state, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_string_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    unsigned long long t0 = net_now_ms();
    CURLcode code = curl_easy_perform(curl);
    unsigned long long elapsed = net_now_ms() - t0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, code_out);
    if (headers) curl_slist_free_all(headers);
    log_http_result(state, "get-reuse", url, code, *code_out, elapsed, strlen(response));
    return code;
}

CURLcode http_download(AppState *state, const char *url, const char *path, long *code_out) {
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
    ctx.write_cap = DOWNLOAD_WRITE_BUF_SIZE;
    ctx.write_buf = (char *)malloc(ctx.write_cap);

    struct curl_slist *headers = setup_headers(curl, state, url);
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, DOWNLOAD_CURL_BUF_SIZE);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 256L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 45L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, download_progress_cb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
    unsigned long long t0 = net_now_ms();
    CURLcode code = curl_easy_perform(curl);
    if (!flush_write_buf(&ctx) && code == CURLE_OK) code = CURLE_WRITE_ERROR;
    unsigned long long elapsed = net_now_ms() - t0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, code_out);
    curl_easy_cleanup(curl);
    if (headers) curl_slist_free_all(headers);
    free(ctx.write_buf);
    sceIoClose(fd);
    SceIoStat st;
    memset(&st, 0, sizeof(st));
    size_t bytes = sceIoGetstat(path, &st) >= 0 ? (size_t)st.st_size : 0;
    log_http_result(state, "download", url, code, *code_out, elapsed, bytes);
    return code;
}

CURLcode http_download_quiet(AppState *state, const char *url, const char *path, long *code_out) {
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
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    unsigned long long t0 = net_now_ms();
    CURLcode code = curl_easy_perform(curl);
    unsigned long long elapsed = net_now_ms() - t0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, code_out);
    curl_easy_cleanup(curl);
    if (headers) curl_slist_free_all(headers);
    sceIoClose(fd);
    SceIoStat st;
    memset(&st, 0, sizeof(st));
    size_t bytes = sceIoGetstat(path, &st) >= 0 ? (size_t)st.st_size : 0;
    log_http_result(state, "download-quiet", url, code, *code_out, elapsed, bytes);
    return code;
}
