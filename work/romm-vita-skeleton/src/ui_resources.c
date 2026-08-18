#include "ui_resources.h"

BitmapFont *g_font = NULL;
vita2d_texture *g_logo_texture = NULL;
vita2d_texture *g_platform_textures[BUILTIN_PLATFORM_ICON_COUNT] = { NULL };

void ui_resources_unload_platform_icons(void) {
    for (int i = 0; i < BUILTIN_PLATFORM_ICON_COUNT; i++) {
        if (g_platform_textures[i]) {
            vita2d_free_texture(g_platform_textures[i]);
            g_platform_textures[i] = NULL;
        }
    }
}
