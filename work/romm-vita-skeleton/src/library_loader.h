#ifndef VITA_ROMM_LIBRARY_LOADER_H
#define VITA_ROMM_LIBRARY_LOADER_H

#include "app_types.h"

void load_platforms_and_roms(AppState *state);
void load_games(AppState *state, int platform_index);
void lock_games(AppState *state);
void unlock_games(AppState *state);

#endif
