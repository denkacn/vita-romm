#include "app.h"
#include "app_state.h"
#include "config.h"
#include "cover_cache.h"
#include "input.h"
#include "library_loader.h"
#include "net.h"
#include "ui.h"
#include "ui_resources.h"
#include "vita_fs.h"

static AppState state;

static void load_builtin_platform_icons(void) {
    g_platform_textures[ICON_PSVITA] = vita2d_load_PNG_file("app0:assets/platforms/platform_psvita.png");
    g_platform_textures[ICON_PSP] = vita2d_load_PNG_file("app0:assets/platforms/platform_psp.png");
    g_platform_textures[ICON_PSX] = vita2d_load_PNG_file("app0:assets/platforms/platform_psx.png");
    g_platform_textures[ICON_GBA] = vita2d_load_PNG_file("app0:assets/platforms/platform_gba.png");
    g_platform_textures[ICON_GB] = vita2d_load_PNG_file("app0:assets/platforms/platform_gb.png");
    g_platform_textures[ICON_GBC] = vita2d_load_PNG_file("app0:assets/platforms/platform_gbc.png");
    g_platform_textures[ICON_NES] = vita2d_load_PNG_file("app0:assets/platforms/platform_nes.png");
    g_platform_textures[ICON_SNES] = vita2d_load_PNG_file("app0:assets/platforms/platform_snes.png");
    g_platform_textures[ICON_GENESIS] = vita2d_load_PNG_file("app0:assets/platforms/platform_genesis.png");
    g_platform_textures[ICON_N64] = vita2d_load_PNG_file("app0:assets/platforms/platform_n64.png");
    g_platform_textures[ICON_FALLBACK] = vita2d_load_PNG_file("app0:assets/platforms/platform_fallback.png");
}

static void app_release_library_textures(void) {
    for (int i = 0; i < state.library.count; i++) {
        if (state.library.roms[i].cover) vita2d_free_texture(state.library.roms[i].cover);
    }
    for (int i = 0; i < state.catalog.count; i++) {
        if (state.catalog.platforms[i].icon) vita2d_free_texture(state.catalog.platforms[i].icon);
    }
}

static bool app_init(BitmapFont *font) {
    init_defaults(&state);
    state.library_worker.lock = sceKernelCreateSema("vita_romm_games_lock", 0, 1, 1, NULL);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    ensure_config(&state);
    debug_log_event(&state, "app init", "config ready");
    init_network(&state);
    refresh_network(&state);
    curl_global_init(CURL_GLOBAL_DEFAULT);
    debug_log_event(&state, "app init", "curl ready");

    vita2d_init();
    vita2d_set_clear_color(COLOR_BG);
    memset(font, 0, sizeof(*font));
    font->atlas = vita2d_load_PNG_file(FONT_ATLAS_PATH);
    if (!font->atlas) return false;

    g_font = font;
    g_logo_texture = vita2d_load_PNG_file("app0:sce_sys/icon0.png");
    load_builtin_platform_icons();
    debug_log_event(&state, "app init", "ui resources ready");
    return true;
}

static void app_shutdown(BitmapFont *font) {
    app_release_library_textures();
    ui_resources_unload_platform_icons();
    if (g_logo_texture) vita2d_free_texture(g_logo_texture);
    if (font->atlas) vita2d_free_texture(font->atlas);
    g_logo_texture = NULL;
    g_font = NULL;
    vita2d_fini();
    curl_global_cleanup();
}

static void app_boot_library(BitmapFont *font) {
    app_state_sync_views(&state);
    draw_ui(font, &state);
    sceDisplayWaitVblankStart();
    debug_log_event(&state, "boot splash", "delay 3000ms");
    sceKernelDelayThread(3000000);
    debug_log_event(&state, "boot library", "loading platforms");
    load_platforms_and_roms(&state);
}

static void app_frame(BitmapFont *font, unsigned int *old_buttons) {
    SceCtrlData pad;
    memset(&pad, 0, sizeof(pad));
    sceCtrlPeekBufferPositive(0, &pad, 1);
    unsigned int pressed = pad.buttons & ~*old_buttons;
    *old_buttons = pad.buttons;

    input_handle(&state, font, pressed);
    if (!state.busy && state.view == VIEW_GAMES) ensure_current_cover_loading(&state);

    app_state_sync_views(&state);
    draw_ui(font, &state);
    sceDisplayWaitVblankStart();
}

int app_main(void) {
    BitmapFont font;
    if (!app_init(&font)) {
        sceKernelExitProcess(1);
        return 1;
    }

    app_boot_library(&font);
    unsigned int old_buttons = 0;
    while (true) app_frame(&font, &old_buttons);

    app_shutdown(&font);
    sceKernelExitProcess(0);
    return 0;
}
