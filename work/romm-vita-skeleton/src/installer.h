#ifndef VITA_ROMM_INSTALLER_H
#define VITA_ROMM_INSTALLER_H

#include "app_types.h"

void update_rom_status(AppState *state, int index);
void download_selected_rom(AppState *state);

#endif
