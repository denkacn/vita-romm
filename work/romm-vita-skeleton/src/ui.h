#ifndef VITA_ROMM_UI_H
#define VITA_ROMM_UI_H

#include "app_types.h"

void draw_ui(BitmapFont *font, const AppState *state);
int font_size_index(float scale);
void draw_text(BitmapFont *font, int x, int y, float scale, unsigned int color, const char *text);

#endif
