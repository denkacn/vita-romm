#include "ui_internal.h"
#include "ui_resources.h"

void ui_draw_panel(int x, int y, int w, int h, unsigned int color) {
    vita2d_draw_rectangle(x, y, w, h, color);
}

void ui_draw_panel_border(int x, int y, int w, int h, unsigned int color) {
    vita2d_draw_rectangle(x, y, w, 2, color);
    vita2d_draw_rectangle(x, y + h - 2, w, 2, color);
    vita2d_draw_rectangle(x, y, 2, h, color);
    vita2d_draw_rectangle(x + w - 2, y, 2, h, color);
}

void ui_draw_texture_fit(vita2d_texture *tex, int x, int y, int w, int h) {
    if (!tex || w <= 0 || h <= 0) return;
    int tex_w = vita2d_texture_get_width(tex);
    int tex_h = vita2d_texture_get_height(tex);
    if (tex_w <= 0 || tex_h <= 0) return;
    float sx = (float)w / (float)tex_w;
    float sy = (float)h / (float)tex_h;
    float s = sx < sy ? sx : sy;
    int dw = (int)(tex_w * s);
    int dh = (int)(tex_h * s);
    vita2d_draw_texture_scale(tex, x + (w - dw) / 2, y + (h - dh) / 2, s, s);
}

int ui_text_width(float scale, const char *text) {
    if (!text) return 0;
    int size_index = font_size_index(scale);
    int width = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        unsigned char ch = *p;
        if (ch < FONT_ATLAS_FIRST || ch >= FONT_ATLAS_FIRST + FONT_ATLAS_COUNT) ch = '?';
        width += FONT_ATLAS_GLYPHS[size_index][ch - FONT_ATLAS_FIRST].advance;
    }
    return width;
}

void ui_draw_text_center(BitmapFont *font, int center_x, int y, float scale, unsigned int color, const char *text) {
    draw_text(font, center_x - ui_text_width(scale, text) / 2, y, scale, color, text);
}

void ui_draw_text_limit(BitmapFont *font, int x, int y, float scale, unsigned int color, const char *text, int max_chars) {
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

void ui_draw_text_limit_center(BitmapFont *font, int center_x, int y, float scale, unsigned int color, const char *text, int max_chars) {
    if (!text) return;
    char buf[180];
    int len = (int)strlen(text);
    if (len <= max_chars || max_chars >= (int)sizeof(buf) - 4) {
        ui_draw_text_center(font, center_x, y, scale, color, text);
        return;
    }
    int n = max_chars - 3;
    if (n < 1) n = 1;
    snprintf(buf, sizeof(buf), "%.*s...", n, text);
    ui_draw_text_center(font, center_x, y, scale, color, buf);
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
        ui_draw_panel_border(sx, sy, 18, 18, fg);
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

void ui_draw_button_hint(BitmapFont *font, int x, int y, const char *button, const char *label, unsigned int button_color) {
    int bw = button_hint_width(button);
    ui_draw_panel(x, y, bw, 32, button_color);
    draw_button_icon(font, x, y, bw, button, COLOR_BG);
    draw_text(font, x + bw + 12, y + 23, 0.58f, COLOR_TEXT, label);
}

void ui_draw_badge(BitmapFont *font, int x, int y, const char *label, unsigned int bg, unsigned int fg) {
    ui_draw_panel(x, y, 150, 34, bg);
    draw_text(font, x + 14, y + 25, 0.58f, fg, label);
}

void ui_format_size(char *dst, size_t dst_size, long long bytes) {
    if (bytes <= 0) {
        snprintf(dst, dst_size, "Size unknown");
        return;
    }
    double value = (double)bytes;
    const char *unit = "B";
    if (value >= 1024.0) {
        value /= 1024.0;
        unit = "KB";
    }
    if (value >= 1024.0) {
        value /= 1024.0;
        unit = "MB";
    }
    if (value >= 1024.0) {
        value /= 1024.0;
        unit = "GB";
    }
    snprintf(dst, dst_size, "%.1f %s", value, unit);
}

void ui_draw_progress_bar(BitmapFont *font, const AppState *state, int x, int y, int w) {
    int fill = 0;
    if (state->progress.total > 0) fill = (state->progress.current * w) / state->progress.total;
    if (fill < 0) fill = 0;
    if (fill > w) fill = w;
    ui_draw_panel(x, y, w, 14, COLOR_CARD);
    ui_draw_panel(x, y, fill, 14, COLOR_ACCENT);
    if (state->progress.total > 0) {
        char progress[64];
        snprintf(progress, sizeof(progress), "%d / %d", state->progress.current, state->progress.total);
        draw_text(font, x + w + 18, y + 13, 0.58f, COLOR_MUTED, progress);
    }
}

void ui_draw_platform_icon(BitmapFont *font, const PlatformDef *p, int x, int y, int size, unsigned int accent) {
    (void)accent;
    vita2d_texture *icon = NULL;
    if (p->icon) icon = p->icon;
    else if (p->icon_index >= 0 && p->icon_index < BUILTIN_PLATFORM_ICON_COUNT) icon = g_platform_textures[p->icon_index];
    if (icon) {
        ui_draw_texture_fit(icon, x, y, size, size);
    } else {
        ui_draw_text_limit(font, x + 18, y + size / 2 + 10, 0.72f, COLOR_BG, p->slug, 8);
    }
}

void draw_ui(BitmapFont *font, const AppState *state) {
    vita2d_start_drawing();
    vita2d_clear_screen();
    ui_draw_panel(0, 0, 960, 544, COLOR_BG);
    if (state->view == VIEW_LOADING) {
        ui_draw_loading(font, state);
    } else {
        ui_draw_top(font, state);
        if (state->view == VIEW_PLATFORMS) ui_draw_platforms(font, state);
        else if (state->view == VIEW_GAME_LIST) ui_draw_game_list(font, state);
        else ui_draw_games(font, state);
        ui_draw_status_footer(font, state);
    }
    vita2d_end_drawing();
    vita2d_swap_buffers();
}





