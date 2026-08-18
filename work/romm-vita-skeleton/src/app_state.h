#ifndef VITA_ROMM_APP_STATE_H
#define VITA_ROMM_APP_STATE_H

#include "app_types.h"

void app_state_sync_views(AppState *state);
PlatformDef *app_selected_platform(AppState *state);
RomItem *app_selected_rom(AppState *state);
const PlatformDef *app_current_platform(const AppState *state);
const RomItem *app_current_rom(const AppState *state);

#endif
