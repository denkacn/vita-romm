#ifndef VITA_ROMM_LIVEAREA_H
#define VITA_ROMM_LIVEAREA_H

#include "app_types.h"

int refresh_vita_livearea_for_title(const AppState *state, const char *title_id);
void refresh_livearea_from_main(AppState *state);
void rom_download_path(const AppState *state, const RomItem *rom, int platform_index, char *path, size_t path_size);
void platform_cache_dir(const PlatformDef *platform, char *path, size_t path_size);
void rom_folder_name(const RomItem *rom, char *folder, size_t folder_size);
bool extension_matches_platform(const char *name, const PlatformDef *platform);
bool psx_file_is_direct_install(const char *name);
void psx_rom_file_destination(const AppState *state, const RomItem *rom, const RomFileItem *file_item, int platform_index, char *path, size_t path_size);
bool validate_vita_install(const AppState *state, const char *title_id, char *missing, size_t missing_size);

#endif
