#include "app_state.h"

void app_state_sync_views(AppState *state) {
    state->library.loading = state->library_worker.running;
    state->library.loading_more = state->library_worker.loading_more;
}

PlatformDef *app_selected_platform(AppState *state) {
    if (!state || state->catalog.selected < 0 || state->catalog.selected >= state->catalog.count) return NULL;
    return &state->catalog.platforms[state->catalog.selected];
}

RomItem *app_selected_rom(AppState *state) {
    if (!state || state->library.selected < 0 || state->library.selected >= state->library.count) return NULL;
    return &state->library.roms[state->library.selected];
}

const PlatformDef *app_current_platform(const AppState *state) {
    if (!state || state->catalog.selected < 0 || state->catalog.selected >= state->catalog.count) return NULL;
    return &state->catalog.platforms[state->catalog.selected];
}

const RomItem *app_current_rom(const AppState *state) {
    if (!state || state->library.selected < 0 || state->library.selected >= state->library.count) return NULL;
    return &state->library.roms[state->library.selected];
}
