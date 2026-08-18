#ifndef VITA_ROMM_COVER_CACHE_H
#define VITA_ROMM_COVER_CACHE_H

#include "app_types.h"

void load_current_cover_from_cache(AppState *state);
void ensure_current_cover_loading(AppState *state);
void load_cached_platform_icon(AppState *state, PlatformDef *platform);

#endif
