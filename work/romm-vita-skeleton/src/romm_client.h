#ifndef VITA_ROMM_CLIENT_H
#define VITA_ROMM_CLIENT_H

#include "app_types.h"

CURLcode romm_get_platform_roms_page_fast(AppState *state, CURL *curl, int platform_id, int limit, int offset, char *response, long *http_out);
unsigned long long vita_now_ms(void);
int romm_get_platform_rom_count(AppState *state, const PlatformDef *platform, long *http_out);
void make_absolute_url(char *dst, size_t dst_size, const AppState *state, const char *maybe_path);
bool load_rom_metadata_from_detail(AppState *state, RomItem *rom);
int load_rom_file_list(AppState *state, const RomItem *rom, RomFileItem *files, int max_files);

#endif
