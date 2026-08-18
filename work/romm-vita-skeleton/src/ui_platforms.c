#include "ui_internal.h"

static void draw_platform_card(BitmapFont *font, const AppState *state, int i, int x, int y) {
    const bool selected = i == state->catalog.selected;
    const PlatformDef *p = &state->catalog.platforms[i];
    unsigned int bg = selected ? COLOR_CARD_ACTIVE : COLOR_CARD;
    unsigned int edge = selected ? COLOR_ACCENT : RGBA8(48, 58, 72, 255);

    const int size = 228;
    ui_draw_text_limit_center(font, x + size / 2, y - 24, 0.72f, COLOR_TEXT, p->label, 24);
    ui_draw_panel(x, y, size, size, bg);
    ui_draw_panel_border(x, y, size, size, edge);
    ui_draw_platform_icon(font, p, x + 11, y + 11, 206, edge);

    char count[80];
    snprintf(count, sizeof(count), "%d games", p->rom_count);
    ui_draw_text_center(font, x + size / 2, y + size + 38, 0.64f, COLOR_GOOD, count);
}

void ui_draw_platforms(BitmapFont *font, const AppState *state) {
    draw_text(font, 56, 122, 1.0f, COLOR_TEXT, "Library");

    if (state->catalog.count <= 0) {
        draw_text(font, 320, 270, 0.84f, COLOR_WARN, "No platforms with games");
        return;
    }

    const int card_w = 228;
    const int gap = 58;
    const int center_x = 480 - card_w / 2;
    const int y = 188;
    for (int i = 0; i < state->catalog.count; i++) {
        int x = center_x + (i - state->catalog.selected) * (card_w + gap);
        if (x < -card_w - 20 || x > 980) continue;
        draw_platform_card(font, state, i, x, y);
    }

    if (state->busy && state->progress.total > 0) {
        ui_draw_progress_bar(font, state, 170, 462, 560);
    }
}
