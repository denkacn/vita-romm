#ifndef VITA_ROMM_PARSER_H
#define VITA_ROMM_PARSER_H

#include "app_types.h"

int parse_platform_defs(const char *json, PlatformDef *platforms, int max_platforms);
int extract_collection_count(const char *json);
bool find_cover_url(const char *start, const char *end, char *dst, size_t dst_size);
long long parse_rom_total_size(const char *json);
int parse_rom_files(const char *json, RomFileItem *files, int max_files);
int parse_rom_items(const char *json, const char *platform_slug, int expected_platform_id, RomItem *roms, int max_roms);
int count_roms_for_platform(const char *json, int expected_platform_id);
int count_rom_objects(const char *json);

#endif
