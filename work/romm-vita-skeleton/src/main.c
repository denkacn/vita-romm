#include "app_internal.h"

#include "util.c"
#include "fs.c"
#include "config.c"
#include "net.c"
#include "json_parse.c"
#include "romm_api.c"
#include "zip_layout.c"
#include "livearea.c"
#include "installer.c"
#include "ui_font.c"
#include "ui.c"
int main(void) {
    AppState state;
    init_defaults(&state);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    ensure_config(&state);
    init_network(&state);
    refresh_network(&state);
    curl_global_init(CURL_GLOBAL_DEFAULT);

    vita2d_init();
    vita2d_set_clear_color(COLOR_BG);
    BitmapFont font;
    memset(&font, 0, sizeof(font));
    font.atlas = vita2d_load_PNG_file(FONT_ATLAS_PATH);
    if (!font.atlas) {
        sceKernelExitProcess(1);
        return 1;
    }
    g_font = &font;
    g_logo_texture = vita2d_load_PNG_file("app0:sce_sys/icon0.png");
    g_platform_textures[0] = vita2d_load_PNG_file("app0:assets/platforms/platform_psvita.png");
    g_platform_textures[1] = vita2d_load_PNG_file("app0:assets/platforms/platform_psp.png");

    draw_ui(&font, &state);
    sceDisplayWaitVblankStart();
    sceKernelDelayThread(3000000);
    load_platforms_and_roms(&state);

    SceCtrlData pad;
    unsigned int old_buttons = 0;
    bool running = true;
    while (running) {
        memset(&pad, 0, sizeof(pad));
        sceCtrlPeekBufferPositive(0, &pad, 1);
        unsigned int pressed = pad.buttons & ~old_buttons;
        old_buttons = pad.buttons;

        if (!state.busy && state.view == VIEW_PLATFORMS) {
            if (pressed & (SCE_CTRL_LEFT | SCE_CTRL_RIGHT)) {
                if (pressed & SCE_CTRL_LEFT) state.selected_platform = (state.selected_platform + PLATFORM_COUNT - 1) % PLATFORM_COUNT;
                if (pressed & SCE_CTRL_RIGHT) state.selected_platform = (state.selected_platform + 1) % PLATFORM_COUNT;
            }
            if (pressed & SCE_CTRL_CROSS) {
                draw_ui(&font, &state);
                load_games(&state, state.selected_platform);
            }
            if (pressed & SCE_CTRL_TRIANGLE) {
                draw_ui(&font, &state);
                refresh_livearea_from_main(&state);
            }
        } else if (!state.busy && state.view == VIEW_GAMES) {
            if ((pressed & SCE_CTRL_LEFT) && state.rom_count > 0) {
                state.selected_rom = (state.selected_rom + state.rom_count - 1) % state.rom_count;
                load_current_cover(&state);
            }
            if ((pressed & SCE_CTRL_RIGHT) && state.rom_count > 0) {
                state.selected_rom = (state.selected_rom + 1) % state.rom_count;
                load_current_cover(&state);
            }
            if (pressed & SCE_CTRL_CROSS) {
                draw_ui(&font, &state);
                download_selected_rom(&state);
            }
            if (pressed & SCE_CTRL_SQUARE) {
                state.view = VIEW_GAME_LIST;
            }
            if (pressed & SCE_CTRL_CIRCLE) {
                state.view = VIEW_PLATFORMS;
                state.status[0] = '\0';
                snprintf(state.detail, sizeof(state.detail), "Choose platform");
            }
        } else if (!state.busy && state.view == VIEW_GAME_LIST) {
            if ((pressed & SCE_CTRL_UP) && state.rom_count > 0) {
                state.selected_rom = (state.selected_rom + state.rom_count - 1) % state.rom_count;
            }
            if ((pressed & SCE_CTRL_DOWN) && state.rom_count > 0) {
                state.selected_rom = (state.selected_rom + 1) % state.rom_count;
            }
            if ((pressed & SCE_CTRL_CROSS) && state.rom_count > 0) {
                state.view = VIEW_GAMES;
                load_current_cover(&state);
            }
            if (pressed & (SCE_CTRL_CIRCLE | SCE_CTRL_SQUARE)) {
                state.view = VIEW_GAMES;
                load_current_cover(&state);
            }
        }

        draw_ui(&font, &state);
        sceDisplayWaitVblankStart();
    }

    for (int i = 0; i < state.rom_count; i++) {
        if (state.roms[i].cover) vita2d_free_texture(state.roms[i].cover);
    }
    for (int i = 0; i < PLATFORM_COUNT; i++) {
        if (g_platform_textures[i]) vita2d_free_texture(g_platform_textures[i]);
    }
    if (g_logo_texture) vita2d_free_texture(g_logo_texture);
    if (font.atlas) vita2d_free_texture(font.atlas);
    g_font = NULL;
    vita2d_fini();
    curl_global_cleanup();
    sceKernelExitProcess(0);
    return 0;
}














