#ifndef VITA_ROMM_COMMANDS_H
#define VITA_ROMM_COMMANDS_H

#include "app_types.h"

void command_select_platform(AppState *state, int delta);
void command_open_selected_platform(AppState *state, BitmapFont *font);
void command_refresh_livearea(AppState *state, BitmapFont *font);
void command_browse_game(AppState *state, int delta);
void command_refresh_game_cover(AppState *state, BitmapFont *font);
void command_open_game_list(AppState *state);
void command_back_to_platforms(AppState *state);
void command_move_game_list(AppState *state, int delta);
void command_select_game_from_list(AppState *state);
void command_close_game_list(AppState *state);
void command_download_selected(AppState *state, BitmapFont *font);

#endif
