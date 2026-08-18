#include "ui_internal.h"

static void draw_cover(BitmapFont *font, const RomItem *rom, int x, int y, int w, int h) {
    ui_draw_panel(x, y, w, h, COLOR_CARD);
    ui_draw_panel_border(x, y, w, h, RGBA8(48, 58, 72, 255));
    if (rom->cover) {
        ui_draw_texture_fit(rom->cover, x + 16, y + 16, w - 32, h - 32);
    } else {
        ui_draw_panel(x + 32, y + 32, w - 64, h - 64, COLOR_BG);
        draw_text(font, x + 82, y + h / 2, 0.86f, COLOR_MUTED, "No poster");
    }
}

void ui_draw_games(BitmapFont *font, const AppState *state) {
    const PlatformDef *p = &state->catalog.platforms[state->catalog.selected];
    draw_text(font, 42, 112, 0.68f, COLOR_MUTED, p->label);

    if (state->library.count <= 0) {
        if (state->library_worker.running) {
            draw_text(font, 300, 252, 0.9f, COLOR_ACCENT, "Loading games...");
            ui_draw_text_limit(font, 300, 292, 0.58f, COLOR_MUTED, state->detail, 42);
        } else {
            draw_text(font, 300, 270, 0.9f, COLOR_WARN, "No games loaded");
        }
        return;
    }

    const RomItem *rom = &state->library.roms[state->library.selected];
    draw_cover(font, rom, 46, 126, 330, 350);

    ui_draw_panel(410, 126, 506, 350, RGBA8(24, 30, 40, 255));
    ui_draw_panel_border(410, 126, 506, 350, RGBA8(45, 56, 72, 255));
    ui_draw_text_limit(font, 438, 190, 1.15f, COLOR_TEXT, rom->name, 30);

    char size_text[40];
    ui_format_size(size_text, sizeof(size_text), rom->size_bytes);
    char meta[160];
    snprintf(meta, sizeof(meta), "%d / %d    RomM id %d    %s", state->library.selected + 1, state->library.count, rom->id, size_text);
    draw_text(font, 438, 236, 0.68f, COLOR_MUTED, meta);

    int badge_x = 438;
    if (rom->is_downloaded) {
        ui_draw_badge(font, badge_x, 260, "Downloaded", COLOR_CARD_ACTIVE, COLOR_TEXT);
        badge_x += 166;
    }
    if (rom->is_installed) {
        ui_draw_badge(font, badge_x, 260, "Installed", COLOR_GOOD, COLOR_BG);
    }
    if (!rom->is_downloaded && !rom->is_installed) {
        ui_draw_badge(font, badge_x, 260, "Not installed", RGBA8(52, 58, 68, 255), COLOR_MUTED);
    }

    ui_draw_panel(438, 326, 420, 2, RGBA8(45, 56, 72, 255));
    if (state->busy) {
        ui_draw_panel(438, 358, 420, 96, RGBA8(52, 58, 68, 255));
        draw_text(font, 458, 388, 0.68f, COLOR_WARN, "Please wait");
        ui_draw_text_limit(font, 458, 420, 0.58f, COLOR_TEXT, state->status, 34);
        ui_draw_text_limit(font, 458, 446, 0.52f, COLOR_MUTED, state->detail, 42);
    } else if (p->support_level == SUPPORT_PATH_REQUIRED) {
        char line[120];
        snprintf(line, sizeof(line), "Add platform.%s.path to config.txt", p->slug);
        ui_draw_text_limit(font, 438, 362, 0.62f, COLOR_WARN, "Download path is not configured.", 44);
        ui_draw_text_limit(font, 438, 390, 0.54f, COLOR_MUTED, line, 48);
    } else if (p->support_level == SUPPORT_UNTESTED) {
        ui_draw_text_limit(font, 438, 358, 0.54f, COLOR_WARN, "Download path configured, but this platform was not tested on PS Vita.", 56);
        ui_draw_text_limit(font, 438, 386, 0.54f, COLOR_MUTED, "Use with your own emulator setup.", 46);
        ui_draw_text_limit(font, 438, 414, 0.50f, COLOR_MUTED, p->path, 52);
    } else if (p->install_mode == INSTALL_FOLDER || p->install_mode == INSTALL_PSX) {
        ui_draw_text_limit(font, 438, 374, 0.58f, COLOR_MUTED, state->detail, 50);
        ui_draw_text_limit(font, 438, 402, 0.52f, COLOR_MUTED, "Run with your emulator.", 42);
    } else {
        ui_draw_text_limit(font, 438, 374, 0.62f, COLOR_MUTED, state->detail, 46);
    }

    if (!state->busy && p->support_level != SUPPORT_PATH_REQUIRED) {
        ui_draw_button_hint(font, 438, 414, "X", rom->is_installed ? "Reinstall" : "Download/install", COLOR_GOOD);
    }
}

void ui_draw_game_list(BitmapFont *font, const AppState *state) {
    const PlatformDef *p = &state->catalog.platforms[state->catalog.selected];
    draw_text(font, 42, 112, 0.68f, COLOR_MUTED, p->label);
    draw_text(font, 42, 148, 1.0f, COLOR_TEXT, "Game list");

    ui_draw_panel(64, 184, 832, 286, RGBA8(24, 30, 40, 255));
    ui_draw_panel_border(64, 184, 832, 286, RGBA8(45, 56, 72, 255));

    if (state->library.count <= 0) {
        if (state->library_worker.running) {
            draw_text(font, 340, 330, 0.86f, COLOR_ACCENT, "Loading games...");
        } else {
            draw_text(font, 360, 330, 0.86f, COLOR_WARN, "No games loaded");
        }
        return;
    }

    int visible = 7;
    int start = state->library.selected - visible / 2;
    if (start < 0) start = 0;
    if (start > state->library.count - visible) start = state->library.count - visible;
    if (start < 0) start = 0;

    for (int row = 0; row < visible; row++) {
        int idx = start + row;
        if (idx >= state->library.count) break;
        int y = 224 + row * 36;
        const bool selected = idx == state->library.selected;
        if (selected) ui_draw_panel(84, y - 25, 792, 34, COLOR_CARD_ACTIVE);

        char index[24];
        snprintf(index, sizeof(index), "%02d", idx + 1);
        draw_text(font, 108, y, 0.56f, selected ? COLOR_ACCENT : COLOR_MUTED, index);
        char row[220];
        char row_size[40];
        ui_format_size(row_size, sizeof(row_size), state->library.roms[idx].size_bytes);
        snprintf(row, sizeof(row), "%s  [%s]", state->library.roms[idx].name, row_size);
        ui_draw_text_limit(font, 166, y, 0.64f, selected ? COLOR_TEXT : COLOR_MUTED, row, 56);

        int badge_x = 752;
        if (state->library.roms[idx].is_installed) draw_text(font, badge_x, y, 0.54f, COLOR_GOOD, "Installed");
        else if (state->library.roms[idx].is_downloaded) draw_text(font, badge_x, y, 0.54f, COLOR_ACCENT, "Downloaded");
    }
}
