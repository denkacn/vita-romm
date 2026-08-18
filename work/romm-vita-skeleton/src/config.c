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
    sceIoMkdir(PLATFORM_ICON_CACHE_DIR, VITA_DIR_MODE);
    sceIoMkdir(DOWNLOAD_DIR, VITA_DIR_MODE);
    sceIoMkdir(DOWNLOAD_DIR "/psvita", VITA_DIR_MODE);
    sceIoMkdir(DOWNLOAD_DIR "/psp", VITA_DIR_MODE);
    sceIoMkdir(DOWNLOAD_DIR "/psx", VITA_DIR_MODE);
    sceIoMkdir(DOWNLOAD_DIR "/gba", VITA_DIR_MODE);
    sceIoMkdir(DOWNLOAD_DIR "/gb", VITA_DIR_MODE);
    sceIoMkdir(DOWNLOAD_DIR "/gbc", VITA_DIR_MODE);
    sceIoMkdir(DOWNLOAD_DIR "/nes", VITA_DIR_MODE);
    sceIoMkdir(DOWNLOAD_DIR "/snes", VITA_DIR_MODE);
    sceIoMkdir(DOWNLOAD_DIR "/genesis", VITA_DIR_MODE);
    sceIoMkdir(DOWNLOAD_DIR "/n64", VITA_DIR_MODE);
    sceIoMkdir(CONFIG_DIR "/roms", VITA_DIR_MODE);
    sceIoMkdir(CONFIG_DIR "/roms/psvita", VITA_DIR_MODE);
    sceIoMkdir(CONFIG_DIR "/roms/psp", VITA_DIR_MODE);
    sceIoMkdir(CONFIG_DIR "/roms/psx", VITA_DIR_MODE);
    sceIoMkdir(CONFIG_DIR "/roms/gba", VITA_DIR_MODE);
    sceIoMkdir(CONFIG_DIR "/roms/gb", VITA_DIR_MODE);
    sceIoMkdir(CONFIG_DIR "/roms/gbc", VITA_DIR_MODE);
    sceIoMkdir(CONFIG_DIR "/roms/nes", VITA_DIR_MODE);
    sceIoMkdir(CONFIG_DIR "/roms/snes", VITA_DIR_MODE);
    sceIoMkdir(CONFIG_DIR "/roms/genesis", VITA_DIR_MODE);
    sceIoMkdir(CONFIG_DIR "/roms/n64", VITA_DIR_MODE);
}

static void set_platform_path(AppState *state, const char *slug, const char *path) {
    if (!slug || !slug[0] || !path || !path[0]) return;
    for (int i = 0; i < state->platform_path_count; i++) {
        if (strcmp(state->platform_paths[i].slug, slug) == 0) {
            copy_value(state->platform_paths[i].path, sizeof(state->platform_paths[i].path), path);
            return;
        }
    }
    if (state->platform_path_count >= MAX_PLATFORM_PATHS) return;
    PlatformPathConfig *cfg = &state->platform_paths[state->platform_path_count++];
    snprintf(cfg->slug, sizeof(cfg->slug), "%s", slug);
    lowercase_ascii(cfg->slug);
    copy_value(cfg->path, sizeof(cfg->path), path);
}

static const char *get_platform_path_config(const AppState *state, const char *slug) {
    for (int i = 0; i < state->platform_path_count; i++) {
        if (strcmp(state->platform_paths[i].slug, slug) == 0 && state->platform_paths[i].path[0]) {
            return state->platform_paths[i].path;
        }
    }
    return "";
}

static void add_default_platform_paths(AppState *state) {
    set_platform_path(state, "psx", CONFIG_DIR "/roms/psx");
    set_platform_path(state, "gba", CONFIG_DIR "/roms/gba");
    set_platform_path(state, "gb", CONFIG_DIR "/roms/gb");
    set_platform_path(state, "gbc", CONFIG_DIR "/roms/gbc");
    set_platform_path(state, "nes", CONFIG_DIR "/roms/nes");
    set_platform_path(state, "snes", CONFIG_DIR "/roms/snes");
    set_platform_path(state, "genesis", CONFIG_DIR "/roms/genesis");
    set_platform_path(state, "n64", CONFIG_DIR "/roms/n64");
}

static bool slug_matches(const char *slug, const char *value) {
    return strcmp(slug, value) == 0;
}

static bool slug_is_psvita(const char *slug, const char *label) {
    return slug_matches(slug, "psvita") || slug_matches(slug, "vita") || strstr(label, "vita");
}

static bool slug_is_psp(const char *slug, const char *label) {
    return slug_matches(slug, "psp") || strstr(label, "portable");
}

static bool slug_is_psx(const char *slug, const char *label) {
    return slug_matches(slug, "psx") || slug_matches(slug, "ps1") || slug_matches(slug, "playstation") ||
        strcmp(label, "playstation") == 0 || strstr(label, "playstation 1") || strstr(label, "ps1");
}

static bool slug_is_snes(const char *slug, const char *label) {
    return slug_matches(slug, "snes") || slug_matches(slug, "super-nintendo") ||
        slug_matches(slug, "super-nintendo-entertainment-system") || strstr(label, "super nintendo");
}

static void configure_platform(AppState *state, PlatformDef *p) {
    char label_l[96];
    snprintf(label_l, sizeof(label_l), "%s", p->label);
    lowercase_ascii(label_l);

    p->install_mode = INSTALL_FOLDER;
    p->support_level = SUPPORT_PATH_REQUIRED;
    p->icon_index = ICON_FALLBACK;
    p->known = false;
    p->path_configured = false;
    p->path[0] = '\0';

    if (slug_is_psvita(p->slug, label_l)) {
        snprintf(p->slug, sizeof(p->slug), "psvita");
        snprintf(p->label, sizeof(p->label), "PS Vita");
        snprintf(p->path, sizeof(p->path), "%s", state->vita_download_dir);
        p->install_mode = INSTALL_VITA;
        p->support_level = SUPPORT_MANAGED;
        p->icon_index = ICON_PSVITA;
        p->path_configured = true;
        p->known = true;
        return;
    }

    if (slug_is_psp(p->slug, label_l)) {
        snprintf(p->slug, sizeof(p->slug), "psp");
        snprintf(p->label, sizeof(p->label), "PSP");
        snprintf(p->path, sizeof(p->path), "%s", state->psp_install_dir);
        p->install_mode = INSTALL_ADRENALINE_PSP;
        p->support_level = SUPPORT_MANAGED;
        p->icon_index = ICON_PSP;
        p->path_configured = true;
        p->known = true;
        return;
    }

    if (slug_is_psx(p->slug, label_l)) {
        snprintf(p->slug, sizeof(p->slug), "psx");
        snprintf(p->label, sizeof(p->label), "PSX");
        p->install_mode = INSTALL_PSX;
        p->support_level = SUPPORT_VERIFIED;
        p->icon_index = ICON_PSX;
        p->known = true;
    } else if (slug_matches(p->slug, "gba")) {
        p->support_level = SUPPORT_VERIFIED;
        p->icon_index = ICON_GBA;
        p->known = true;
    } else if (slug_matches(p->slug, "gb") || slug_matches(p->slug, "gameboy")) {
        snprintf(p->slug, sizeof(p->slug), "gb");
        p->support_level = SUPPORT_VERIFIED;
        p->icon_index = ICON_GB;
        p->known = true;
    } else if (slug_matches(p->slug, "gbc") || slug_matches(p->slug, "gameboy-color")) {
        snprintf(p->slug, sizeof(p->slug), "gbc");
        p->support_level = SUPPORT_VERIFIED;
        p->icon_index = ICON_GBC;
        p->known = true;
    } else if (slug_matches(p->slug, "nes")) {
        p->support_level = SUPPORT_VERIFIED;
        p->icon_index = ICON_NES;
        p->known = true;
    } else if (slug_is_snes(p->slug, label_l)) {
        snprintf(p->slug, sizeof(p->slug), "snes");
        snprintf(p->label, sizeof(p->label), "SNES");
        p->support_level = SUPPORT_VERIFIED;
        p->icon_index = ICON_SNES;
        p->known = true;
    } else if (slug_matches(p->slug, "genesis") || slug_matches(p->slug, "megadrive") || slug_matches(p->slug, "mega-drive")) {
        snprintf(p->slug, sizeof(p->slug), "genesis");
        p->support_level = SUPPORT_VERIFIED;
        p->icon_index = ICON_GENESIS;
        p->known = true;
    } else if (slug_matches(p->slug, "n64") || slug_matches(p->slug, "nintendo-64")) {
        snprintf(p->slug, sizeof(p->slug), "n64");
        p->support_level = SUPPORT_UNTESTED;
        p->icon_index = ICON_N64;
        p->known = true;
    }

    const char *path = get_platform_path_config(state, p->slug);
    if (path[0]) {
        snprintf(p->path, sizeof(p->path), "%s", path);
        p->path_configured = true;
        if (!p->known) p->support_level = SUPPORT_UNTESTED;
    } else {
        p->support_level = SUPPORT_PATH_REQUIRED;
    }
}

static void init_defaults(AppState *state) {
    memset(state, 0, sizeof(*state));
    state->view = VIEW_LOADING;
    state->games_worker_thread = -1;
    state->games_lock = -1;
    state->cover_worker_thread = -1;
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
    snprintf(state->psx_pbp_dir, sizeof(state->psx_pbp_dir), "ux0:pspemu/PSP/GAME");
    add_default_platform_paths(state);
    snprintf(state->status, sizeof(state->status), "Starting");
    snprintf(state->detail, sizeof(state->detail), "Preparing Vita RomM");
}

static void ensure_config(AppState *state) {
    mkdirs();
    SceUID fd = sceIoOpen(CONFIG_PATH, SCE_O_RDONLY, 0);
    if (fd >= 0) {
        char data[4096];
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
                else if (starts_with(line, "platform.psx.pbp_path=")) copy_value(state->psx_pbp_dir, sizeof(state->psx_pbp_dir), line + 22);
                else if (starts_with(line, "auto_refresh_livearea=")) state->auto_refresh_livearea = atoi(line + 22) != 0;
                else if (starts_with(line, "platform.")) {
                    char *path_key = strstr(line, ".path=");
                    if (path_key) {
                        char slug[40];
                        size_t n = (size_t)(path_key - (line + 9));
                        if (n >= sizeof(slug)) n = sizeof(slug) - 1;
                        memcpy(slug, line + 9, n);
                        slug[n] = '\0';
                        lowercase_ascii(slug);
                        set_platform_path(state, slug, path_key + 6);
                    }
                }
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
        if (!state->psx_pbp_dir[0]) snprintf(state->psx_pbp_dir, sizeof(state->psx_pbp_dir), "%s", state->psp_game_dir);
        return;
    }

    fd = sceIoOpen(CONFIG_PATH, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, VITA_FILE_MODE);
    if (fd >= 0) {
        char cfg[4096];
        snprintf(cfg, sizeof(cfg),
            "server_url=http://example.com/\n"
            "api_token=\n"
            "psvita_install_dir=%s\n"
            "psvita_download_dir=%s\n"
            "psvita_app_dir=%s\n"
            "psvita_patch_dir=%s\n"
            "psvita_repatch_dir=%s\n"
            "psvita_addcont_dir=%s\n"
            "psvita_license_dir=%s\n"
            "psp_install_dir=%s\n"
            "psp_game_dir=%s\n"
            "psp_download_dir=%s\n"
            "platform.psx.path=%s\n"
            "platform.psx.pbp_path=%s\n"
            "platform.gba.path=%s\n"
            "platform.gb.path=%s\n"
            "platform.gbc.path=%s\n"
            "platform.nes.path=%s\n"
            "platform.snes.path=%s\n"
            "platform.genesis.path=%s\n"
            "platform.n64.path=%s\n"
            "auto_refresh_livearea=0\n",
            state->vita_install_dir,
            state->vita_download_dir,
            state->vita_app_dir,
            state->vita_patch_dir,
            state->vita_repatch_dir,
            state->vita_addcont_dir,
            state->vita_license_dir,
            state->psp_install_dir,
            state->psp_game_dir,
            state->psp_download_dir,
            get_platform_path_config(state, "psx"),
            state->psx_pbp_dir,
            get_platform_path_config(state, "gba"),
            get_platform_path_config(state, "gb"),
            get_platform_path_config(state, "gbc"),
            get_platform_path_config(state, "nes"),
            get_platform_path_config(state, "snes"),
            get_platform_path_config(state, "genesis"),
            get_platform_path_config(state, "n64"));
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

