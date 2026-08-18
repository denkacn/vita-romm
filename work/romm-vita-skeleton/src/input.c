#include "input.h"
#include "commands.h"

static void input_platforms(AppState *state, BitmapFont *font, unsigned int pressed) {
    if (pressed & (SCE_CTRL_LEFT | SCE_CTRL_LTRIGGER)) command_select_platform(state, -1);
    if (pressed & (SCE_CTRL_RIGHT | SCE_CTRL_RTRIGGER)) command_select_platform(state, 1);
    if (pressed & SCE_CTRL_CROSS) command_open_selected_platform(state, font);
    if (pressed & SCE_CTRL_TRIANGLE) command_refresh_livearea(state, font);
}

static void input_games(AppState *state, BitmapFont *font, unsigned int pressed) {
    if (pressed & SCE_CTRL_LEFT) command_browse_game(state, -1);
    if (pressed & SCE_CTRL_RIGHT) command_browse_game(state, 1);
    if (pressed & SCE_CTRL_CROSS) command_download_selected(state, font);
    if (pressed & SCE_CTRL_SQUARE) command_open_game_list(state);
    if (pressed & SCE_CTRL_CIRCLE) command_back_to_platforms(state);
}

static void input_game_list(AppState *state, unsigned int pressed) {
    if (pressed & SCE_CTRL_UP) command_move_game_list(state, -1);
    if (pressed & SCE_CTRL_DOWN) command_move_game_list(state, 1);
    if (pressed & SCE_CTRL_CROSS) command_select_game_from_list(state);
    if (pressed & (SCE_CTRL_CIRCLE | SCE_CTRL_SQUARE)) command_close_game_list(state);
}

void input_handle(AppState *state, BitmapFont *font, unsigned int pressed) {
    if (state->busy) return;
    if (state->view == VIEW_PLATFORMS) input_platforms(state, font, pressed);
    else if (state->view == VIEW_GAMES) input_games(state, font, pressed);
    else if (state->view == VIEW_GAME_LIST) input_game_list(state, pressed);
}
