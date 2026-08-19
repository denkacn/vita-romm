#include "ui_internal.h"
#include "ui_resources.h"
#include "util.h"

void ui_draw_top(BitmapFont *font, const AppState *state) {
    ui_draw_panel(0, 0, 960, 70, COLOR_BAR);
    ui_draw_panel(0, 68, 960, 2, COLOR_ACCENT);
    draw_text(font, 34, 47, 1.15f, COLOR_TEXT, APP_TITLE);
    draw_text(font, 236, 46, 0.64f, COLOR_ACCENT, "v" ROMM_VITA_VERSION);

    unsigned int net_color = starts_with(state->network_status, "Connected") ? COLOR_GOOD : COLOR_WARN;
    draw_text(font, 666, 43, 0.58f, net_color, state->network_status);
    if (state->ip_address[0]) draw_text(font, 800, 43, 0.58f, COLOR_MUTED, state->ip_address);
}

void ui_draw_loading(BitmapFont *font, const AppState *state) {
    (void)state;
    ui_draw_panel(0, 0, 960, 544, COLOR_BG);
    ui_draw_panel(380, 108, 200, 200, RGBA8(24, 30, 40, 255));
    ui_draw_panel_border(380, 108, 200, 200, COLOR_ACCENT);
    if (g_logo_texture) {
        ui_draw_texture_fit(g_logo_texture, 408, 136, 144, 144);
    } else {
        draw_text(font, 432, 226, 1.9f, COLOR_ACCENT, "VR");
    }

    draw_text(font, 374, 368, 1.45f, COLOR_TEXT, "VITA ROMM");
    draw_text(font, 430, 406, 0.72f, COLOR_ACCENT, "v" ROMM_VITA_VERSION);
    ui_draw_panel(342, 442, 276, 10, COLOR_CARD);
    ui_draw_panel(342, 442, 184, 10, COLOR_ACCENT);
}

void ui_draw_status_footer(BitmapFont *font, const AppState *state) {
    ui_draw_panel(0, 486, 960, 58, RGBA8(13, 16, 22, 255));
    ui_draw_panel(0, 486, 960, 2, RGBA8(45, 56, 72, 255));

    if (state->busy) {
        ui_draw_text_limit(font, 42, 522, 0.58f, COLOR_WARN, state->status, 54);
        return;
    }

    if (state->view == VIEW_PLATFORMS) {
        ui_draw_button_hint(font, 42, 500, "L/R", "Select", COLOR_ACCENT);
        ui_draw_button_hint(font, 216, 500, "X", "Open", COLOR_GOOD);
        ui_draw_button_hint(font, 358, 500, "TRI", "Refresh LiveArea", COLOR_WARN);
        if (state->catalog.count > 1) {
            char index_text[40];
            snprintf(index_text, sizeof(index_text), "%d / %d", state->catalog.selected + 1, state->catalog.count);
            draw_text(font, 852, 522, 0.58f, COLOR_MUTED, index_text);
        }
    } else if (state->view == VIEW_GAMES) {
        const PlatformDef *p = &state->catalog.platforms[state->catalog.selected];
        ui_draw_button_hint(font, 42, 500, "L/R", "Browse", COLOR_ACCENT);
        ui_draw_button_hint(font, 216, 500, "SQ", "List", COLOR_ACCENT);
        if (p->support_level != SUPPORT_PATH_REQUIRED) {
            ui_draw_button_hint(font, 350, 500, "X", "Install", COLOR_GOOD);
            ui_draw_button_hint(font, 502, 500, "SEL", "Poster", COLOR_ACCENT);
            ui_draw_button_hint(font, 676, 500, "O", "Back", COLOR_WARN);
        } else {
            ui_draw_button_hint(font, 350, 500, "SEL", "Poster", COLOR_ACCENT);
            ui_draw_button_hint(font, 524, 500, "O", "Back", COLOR_WARN);
        }
    } else if (state->view == VIEW_GAME_LIST) {
        ui_draw_button_hint(font, 42, 500, "U/D", "Move", COLOR_ACCENT);
        ui_draw_button_hint(font, 214, 500, "X", "Select", COLOR_GOOD);
        ui_draw_button_hint(font, 374, 500, "O", "Close", COLOR_WARN);
    }
}
