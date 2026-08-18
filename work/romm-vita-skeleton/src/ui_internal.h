#ifndef VITA_ROMM_UI_INTERNAL_H
#define VITA_ROMM_UI_INTERNAL_H

#include "ui.h"

void ui_draw_panel(int x, int y, int w, int h, unsigned int color);
void ui_draw_panel_border(int x, int y, int w, int h, unsigned int color);
void ui_draw_texture_fit(vita2d_texture *tex, int x, int y, int w, int h);
int ui_text_width(float scale, const char *text);
void ui_draw_text_center(BitmapFont *font, int center_x, int y, float scale, unsigned int color, const char *text);
void ui_draw_text_limit(BitmapFont *font, int x, int y, float scale, unsigned int color, const char *text, int max_chars);
void ui_draw_text_limit_center(BitmapFont *font, int center_x, int y, float scale, unsigned int color, const char *text, int max_chars);
void ui_draw_button_hint(BitmapFont *font, int x, int y, const char *button, const char *label, unsigned int button_color);
void ui_draw_badge(BitmapFont *font, int x, int y, const char *label, unsigned int bg, unsigned int fg);
void ui_format_size(char *dst, size_t dst_size, long long bytes);
void ui_draw_progress_bar(BitmapFont *font, const AppState *state, int x, int y, int w);
void ui_draw_platform_icon(BitmapFont *font, const PlatformDef *p, int x, int y, int size, unsigned int accent);

void ui_draw_top(BitmapFont *font, const AppState *state);
void ui_draw_loading(BitmapFont *font, const AppState *state);
void ui_draw_platforms(BitmapFont *font, const AppState *state);
void ui_draw_games(BitmapFont *font, const AppState *state);
void ui_draw_game_list(BitmapFont *font, const AppState *state);
void ui_draw_status_footer(BitmapFont *font, const AppState *state);

#endif
