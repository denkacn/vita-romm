static void draw_panel(int x, int y, int w, int h, unsigned int color) {
    vita2d_draw_rectangle(x, y, w, h, color);
}

static void draw_panel_border(int x, int y, int w, int h, unsigned int color) {
    vita2d_draw_rectangle(x, y, w, 2, color);
    vita2d_draw_rectangle(x, y + h - 2, w, 2, color);
    vita2d_draw_rectangle(x, y, 2, h, color);
    vita2d_draw_rectangle(x + w - 2, y, 2, h, color);
}

static void draw_text_limit(BitmapFont *font, int x, int y, float scale, unsigned int color, const char *text, int max_chars) {
    if (!text) return;
    char buf[180];
    int len = (int)strlen(text);
    if (len <= max_chars || max_chars >= (int)sizeof(buf) - 4) {
        draw_text(font, x, y, scale, color, text);
        return;
    }
    int n = max_chars - 3;
    if (n < 1) n = 1;
    snprintf(buf, sizeof(buf), "%.*s...", n, text);
    draw_text(font, x, y, scale, color, buf);
}

static int button_hint_width(const char *button) {
    if (button && (strcmp(button, "SQ") == 0 || strcmp(button, "TRI") == 0)) return 42;
    int len = button ? (int)strlen(button) : 1;
    if (len <= 1) return 42;
    if (len == 2) return 52;
    return 64;
}

static void draw_button_icon(BitmapFont *font, int x, int y, int bw, const char *button, unsigned int fg) {
    if (button && strcmp(button, "SQ") == 0) {
        int sx = x + (bw - 18) / 2;
        int sy = y + 7;
        draw_panel_border(sx, sy, 18, 18, fg);
        return;
    }
    if (button && strcmp(button, "TRI") == 0) {
        int cx = x + bw / 2;
        int top = y + 6;
        int left = x + (bw - 24) / 2;
        int right = left + 24;
        for (int i = 0; i < 3; i++) {
            vita2d_draw_line(cx, top + i, left + i, y + 25, fg);
            vita2d_draw_line(cx, top + i, right - i, y + 25, fg);
            vita2d_draw_line(left + i, y + 25 - i, right - i, y + 25 - i, fg);
        }
        return;
    }
    draw_text(font, x + 10, y + 23, 0.54f, fg, button);
}

static void draw_button_hint(BitmapFont *font, int x, int y, const char *button, const char *label, unsigned int button_color) {
    int bw = button_hint_width(button);
    draw_panel(x, y, bw, 32, button_color);
    draw_button_icon(font, x, y, bw, button, COLOR_BG);
    draw_text(font, x + bw + 12, y + 23, 0.58f, COLOR_TEXT, label);
}

static void draw_badge(BitmapFont *font, int x, int y, const char *label, unsigned int bg, unsigned int fg) {
    draw_panel(x, y, 150, 34, bg);
    draw_text(font, x + 14, y + 25, 0.58f, fg, label);
}

static void draw_progress_bar(BitmapFont *font, const AppState *state, int x, int y, int w) {
    int fill = 0;
    if (state->progress_total > 0) fill = (state->progress_current * w) / state->progress_total;
    if (fill < 0) fill = 0;
    if (fill > w) fill = w;
    draw_panel(x, y, w, 14, COLOR_CARD);
    draw_panel(x, y, fill, 14, COLOR_ACCENT);
    if (state->progress_total > 0) {
        char progress[64];
        snprintf(progress, sizeof(progress), "%d / %d", state->progress_current, state->progress_total);
        draw_text(font, x + w + 18, y + 13, 0.58f, COLOR_MUTED, progress);
    }
}

static void draw_platform_icon(BitmapFont *font, int platform_index, int x, int y, int size, unsigned int accent) {
    draw_panel(x, y, size, size, RGBA8(226, 236, 246, 255));
    draw_panel_border(x, y, size, size, accent);
    vita2d_texture *icon = NULL;
    if (platform_index >= 0 && platform_index < PLATFORM_COUNT) icon = g_platform_textures[platform_index];
    if (icon) {
        int tw = vita2d_texture_get_width(icon);
        int th = vita2d_texture_get_height(icon);
        float target = (float)(size - 18);
        float sx = target / (float)tw;
        float sy = target / (float)th;
        float s = sx < sy ? sx : sy;
        int dw = (int)(tw * s);
        int dh = (int)(th * s);
        vita2d_draw_texture_scale(icon, x + (size - dw) / 2, y + (size - dh) / 2, s, s);
    } else if (platform_index == 0) {
        draw_text(font, x + 22, y + size / 2 + 10, 0.86f, COLOR_BG, "VITA");
    } else if (platform_index == 1) {
        draw_text(font, x + 26, y + size / 2 + 10, 0.92f, COLOR_BG, "PSP");
    } else {
        draw_text(font, x + 28, y + size / 2 + 10, 0.92f, COLOR_BG, "ROM");
    }
}

static void draw_top(BitmapFont *font, const AppState *state) {
    draw_panel(0, 0, 960, 70, COLOR_BAR);
    draw_panel(0, 68, 960, 2, COLOR_ACCENT);
    draw_text(font, 34, 47, 1.15f, COLOR_TEXT, APP_TITLE);
    draw_text(font, 236, 46, 0.64f, COLOR_ACCENT, "v" ROMM_VITA_VERSION);

    unsigned int net_color = starts_with(state->network_status, "Connected") ? COLOR_GOOD : COLOR_WARN;
    draw_panel(650, 18, 276, 34, RGBA8(20, 25, 34, 255));
    draw_text(font, 666, 43, 0.58f, net_color, state->network_status);
    if (state->ip_address[0]) draw_text(font, 800, 43, 0.58f, COLOR_MUTED, state->ip_address);
}

static void draw_loading(BitmapFont *font, const AppState *state) {
    (void)state;
    draw_panel(0, 0, 960, 544, COLOR_BG);
    draw_panel(380, 108, 200, 200, RGBA8(24, 30, 40, 255));
    draw_panel_border(380, 108, 200, 200, COLOR_ACCENT);
    if (g_logo_texture) {
        int tw = vita2d_texture_get_width(g_logo_texture);
        int th = vita2d_texture_get_height(g_logo_texture);
        float sx = 144.0f / (float)tw;
        float sy = 144.0f / (float)th;
        float s = sx < sy ? sx : sy;
        int dw = (int)(tw * s);
        int dh = (int)(th * s);
        vita2d_draw_texture_scale(g_logo_texture, 480 - dw / 2, 208 - dh / 2, s, s);
    } else {
        draw_text(font, 432, 226, 1.9f, COLOR_ACCENT, "VR");
    }

    draw_text(font, 374, 368, 1.45f, COLOR_TEXT, "VITA ROMM");
    draw_text(font, 430, 406, 0.72f, COLOR_ACCENT, "v" ROMM_VITA_VERSION);
    draw_panel(342, 442, 276, 10, COLOR_CARD);
    draw_panel(342, 442, 184, 10, COLOR_ACCENT);
}

static void draw_platform_card(BitmapFont *font, const AppState *state, int i, int x, int y) {
    const bool selected = i == state->selected_platform;
    const PlatformDef *p = &state->platforms[i];
    unsigned int bg = selected ? COLOR_CARD_ACTIVE : COLOR_CARD;
    unsigned int edge = selected ? COLOR_ACCENT : RGBA8(48, 58, 72, 255);

    draw_panel(x, y, 250, 264, bg);
    draw_panel_border(x, y, 250, 264, edge);
    draw_text_limit(font, x + 36, y + 48, 0.88f, COLOR_TEXT, p->label, 18);
    draw_platform_icon(font, i, x + 64, y + 76, 122, edge);

    char count[80];
    snprintf(count, sizeof(count), "%d games", p->rom_count);
    draw_text(font, x + 66, y + 232, 0.76f, COLOR_GOOD, count);
}

static void draw_platforms(BitmapFont *font, const AppState *state) {
    draw_text(font, 56, 122, 1.0f, COLOR_TEXT, "Library");
    draw_text(font, 56, 154, 0.64f, COLOR_MUTED, "Choose a platform");

    int cols = PLATFORM_COUNT < 3 ? PLATFORM_COUNT : 3;
    int card_w = 250;
    int gap = 72;
    int start_x = (960 - (cols * card_w + (cols - 1) * gap)) / 2;
    for (int i = 0; i < PLATFORM_COUNT; i++) {
        int col = i % 3;
        int row = i / 3;
        draw_platform_card(font, state, i, start_x + col * (card_w + gap), 184 + row * 284);
    }

    if (state->busy && state->progress_total > 0) {
        draw_progress_bar(font, state, 170, 462, 560);
    }
}

static void draw_cover(BitmapFont *font, const RomItem *rom, int x, int y, int w, int h) {
    draw_panel(x, y, w, h, COLOR_CARD);
    draw_panel_border(x, y, w, h, RGBA8(48, 58, 72, 255));
    if (rom->cover) {
        int tex_w = vita2d_texture_get_width(rom->cover);
        int tex_h = vita2d_texture_get_height(rom->cover);
        float sx = (float)(w - 32) / (float)tex_w;
        float sy = (float)(h - 32) / (float)tex_h;
        float s = sx < sy ? sx : sy;
        int dw = (int)(tex_w * s);
        int dh = (int)(tex_h * s);
        int dx = x + (w - dw) / 2;
        int dy = y + (h - dh) / 2;
        vita2d_draw_texture_scale(rom->cover, dx, dy, s, s);
    } else {
        draw_panel(x + 32, y + 32, w - 64, h - 64, COLOR_BG);
        draw_text(font, x + 82, y + h / 2, 0.86f, COLOR_MUTED, "No poster");
    }
}

static void draw_games(BitmapFont *font, const AppState *state) {
    const PlatformDef *p = &state->platforms[state->selected_platform];
    draw_text(font, 42, 112, 0.68f, COLOR_MUTED, p->label);

    if (state->rom_count <= 0) {
        draw_text(font, 300, 270, 0.9f, COLOR_WARN, "No games loaded");
        return;
    }

    const RomItem *rom = &state->roms[state->selected_rom];
    draw_cover(font, rom, 46, 126, 330, 350);

    draw_panel(410, 126, 506, 350, RGBA8(24, 30, 40, 255));
    draw_panel_border(410, 126, 506, 350, RGBA8(45, 56, 72, 255));
    draw_text_limit(font, 438, 190, 1.15f, COLOR_TEXT, rom->name, 30);

    char meta[120];
    snprintf(meta, sizeof(meta), "%d / %d    RomM id %d", state->selected_rom + 1, state->rom_count, rom->id);
    draw_text(font, 438, 236, 0.68f, COLOR_MUTED, meta);

    int badge_x = 438;
    if (rom->is_downloaded) {
        draw_badge(font, badge_x, 260, "Downloaded", COLOR_CARD_ACTIVE, COLOR_TEXT);
        badge_x += 166;
    }
    if (rom->is_installed) {
        draw_badge(font, badge_x, 260, "Installed", COLOR_GOOD, COLOR_BG);
    }
    if (!rom->is_downloaded && !rom->is_installed) {
        draw_badge(font, badge_x, 260, "Not installed", RGBA8(52, 58, 68, 255), COLOR_MUTED);
    }

    draw_panel(438, 326, 420, 2, RGBA8(45, 56, 72, 255));
    draw_text_limit(font, 438, 374, 0.62f, COLOR_MUTED, state->detail, 46);

    if (state->busy) {
        draw_panel(438, 396, 420, 58, RGBA8(52, 58, 68, 255));
        draw_text(font, 458, 421, 0.62f, COLOR_WARN, "Please wait");
        draw_text_limit(font, 458, 446, 0.58f, COLOR_TEXT, state->status, 34);
    } else {
        draw_button_hint(font, 438, 414, "X", rom->is_installed ? "Reinstall" : "Download/install", COLOR_GOOD);
    }
}

static void draw_game_list(BitmapFont *font, const AppState *state) {
    const PlatformDef *p = &state->platforms[state->selected_platform];
    draw_text(font, 42, 112, 0.68f, COLOR_MUTED, p->label);
    draw_text(font, 42, 148, 1.0f, COLOR_TEXT, "Game list");

    draw_panel(64, 184, 832, 286, RGBA8(24, 30, 40, 255));
    draw_panel_border(64, 184, 832, 286, RGBA8(45, 56, 72, 255));

    if (state->rom_count <= 0) {
        draw_text(font, 360, 330, 0.86f, COLOR_WARN, "No games loaded");
        return;
    }

    int visible = 7;
    int start = state->selected_rom - visible / 2;
    if (start < 0) start = 0;
    if (start > state->rom_count - visible) start = state->rom_count - visible;
    if (start < 0) start = 0;

    for (int row = 0; row < visible; row++) {
        int idx = start + row;
        if (idx >= state->rom_count) break;
        int y = 224 + row * 36;
        const bool selected = idx == state->selected_rom;
        if (selected) draw_panel(84, y - 25, 792, 34, COLOR_CARD_ACTIVE);

        char index[24];
        snprintf(index, sizeof(index), "%02d", idx + 1);
        draw_text(font, 108, y, 0.56f, selected ? COLOR_ACCENT : COLOR_MUTED, index);
        draw_text_limit(font, 166, y, 0.64f, selected ? COLOR_TEXT : COLOR_MUTED, state->roms[idx].name, 50);

        int badge_x = 752;
        if (state->roms[idx].is_installed) draw_text(font, badge_x, y, 0.54f, COLOR_GOOD, "Installed");
        else if (state->roms[idx].is_downloaded) draw_text(font, badge_x, y, 0.54f, COLOR_ACCENT, "Downloaded");
    }
}

static void draw_status_footer(BitmapFont *font, const AppState *state) {
    draw_panel(0, 486, 960, 58, RGBA8(13, 16, 22, 255));
    draw_panel(0, 486, 960, 2, RGBA8(45, 56, 72, 255));

    if (state->busy) {
        draw_text_limit(font, 42, 522, 0.58f, COLOR_WARN, state->status, 54);
        return;
    }

    if (state->view == VIEW_PLATFORMS) {
        draw_button_hint(font, 42, 500, "L/R", "Select", COLOR_ACCENT);
        draw_button_hint(font, 214, 500, "X", "Open", COLOR_GOOD);
        draw_button_hint(font, 356, 500, "TRI", "Refresh LiveArea", COLOR_WARN);
    } else if (state->view == VIEW_GAMES) {
        draw_button_hint(font, 42, 500, "L/R", "Browse", COLOR_ACCENT);
        draw_button_hint(font, 216, 500, "SQ", "List", COLOR_ACCENT);
        draw_button_hint(font, 350, 500, "X", "Install", COLOR_GOOD);
        draw_button_hint(font, 502, 500, "O", "Back", COLOR_WARN);
    } else if (state->view == VIEW_GAME_LIST) {
        draw_button_hint(font, 42, 500, "U/D", "Move", COLOR_ACCENT);
        draw_button_hint(font, 214, 500, "X", "Select", COLOR_GOOD);
        draw_button_hint(font, 374, 500, "O", "Close", COLOR_WARN);
    }
}

static void draw_ui(BitmapFont *font, const AppState *state) {
    vita2d_start_drawing();
    vita2d_clear_screen();
    draw_panel(0, 0, 960, 544, COLOR_BG);
    if (state->view == VIEW_LOADING) {
        draw_loading(font, state);
    } else {
        draw_top(font, state);
        if (state->view == VIEW_PLATFORMS) draw_platforms(font, state);
        else if (state->view == VIEW_GAME_LIST) draw_game_list(font, state);
        else draw_games(font, state);
        draw_status_footer(font, state);
    }
    vita2d_end_drawing();
    vita2d_swap_buffers();
}





