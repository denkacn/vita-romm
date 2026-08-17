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
        char cfg[1400];
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

