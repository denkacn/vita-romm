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

