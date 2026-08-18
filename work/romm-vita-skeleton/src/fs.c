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

static bool dir_exists(const char *path) {
    SceIoStat st;
    memset(&st, 0, sizeof(st));
    return sceIoGetstat(path, &st) >= 0 && SCE_S_ISDIR(st.st_mode);
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
