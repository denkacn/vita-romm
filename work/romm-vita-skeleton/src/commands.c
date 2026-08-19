#include "commands.h"
#include "cover_cache.h"
#include "installer.h"
#include "library_loader.h"
#include "livearea.h"
#include "romm_client.h"
#include "ui.h"

static bool has_games(const AppState *state) {
    return state->library.count > 0;
}

static RomItem *selected_rom(AppState *state) {
    if (!has_games(state)) return NULL;
    return &state->library.roms[state->library.selected];
}

static void select_rom(AppState *state, int delta) {
    if (!has_games(state)) return;
    state->library.selected = (state->library.selected + state->library.count + delta) % state->library.count;
}

void command_select_platform(AppState *state, int delta) {
    if (state->catalog.count <= 0) return;
    state->catalog.selected = (state->catalog.selected + state->catalog.count + delta) % state->catalog.count;
}

void command_open_selected_platform(AppState *state, BitmapFont *font) {
    if (state->catalog.count <= 0) return;
    draw_ui(font, state);
    load_games(state, state->catalog.selected);
}

void command_refresh_livearea(AppState *state, BitmapFont *font) {
    draw_ui(font, state);
    refresh_livearea_from_main(state);
}

void command_browse_game(AppState *state, int delta) {
    select_rom(state, delta);
    load_current_cover_from_cache(state);
}

void command_refresh_game_cover(AppState *state, BitmapFont *font) {
    (void)font;
    if (!has_games(state)) return;
    refresh_current_cover(state);
}

void command_open_game_list(AppState *state) {
    state->view = VIEW_GAME_LIST;
}

void command_back_to_platforms(AppState *state) {
    state->library_worker.generation++;
    state->library_worker.loading_more = false;
    state->library_worker.running = false;
    state->view = VIEW_PLATFORMS;
    state->status[0] = '\0';
    snprintf(state->detail, sizeof(state->detail), "Choose platform");
}

void command_move_game_list(AppState *state, int delta) {
    select_rom(state, delta);
}

void command_select_game_from_list(AppState *state) {
    RomItem *rom = selected_rom(state);
    if (!rom) return;

    state->view = VIEW_GAMES;
    load_rom_metadata_from_detail(state, rom);
    update_rom_status(state, state->library.selected);
    load_current_cover_from_cache(state);
}

void command_close_game_list(AppState *state) {
    command_select_game_from_list(state);
}

void command_download_selected(AppState *state, BitmapFont *font) {
    draw_ui(font, state);
    download_selected_rom(state);
}
