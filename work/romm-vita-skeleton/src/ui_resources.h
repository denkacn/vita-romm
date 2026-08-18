#ifndef VITA_ROMM_UI_RESOURCES_H
#define VITA_ROMM_UI_RESOURCES_H

#include "app_types.h"

extern BitmapFont *g_font;
extern vita2d_texture *g_logo_texture;
extern vita2d_texture *g_platform_textures[BUILTIN_PLATFORM_ICON_COUNT];

void ui_resources_unload_platform_icons(void);

#endif
