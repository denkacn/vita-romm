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
        if (ctx->state->progress_total > 1) {
            snprintf(ctx->state->status, sizeof(ctx->state->status), "Downloading file %d/%d %d%%", ctx->state->progress_current, ctx->state->progress_total, percent);
        } else {
            snprintf(ctx->state->status, sizeof(ctx->state->status), "Downloading %d%%", percent);
        }
        snprintf(ctx->state->detail, sizeof(ctx->state->detail), "%.1f / %.1f MB", mb_now, mb_total);
    } else {
        if (ctx->state->progress_total > 1) {
            snprintf(ctx->state->status, sizeof(ctx->state->status), "Downloading file %d/%d", ctx->state->progress_current, ctx->state->progress_total);
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
