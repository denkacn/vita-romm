#ifndef VITA_ROMM_INSTALLER_INTERNAL_H
#define VITA_ROMM_INSTALLER_INTERNAL_H

#include "config.h"
#include "installer.h"
#include "livearea.h"
#include "net.h"
#include "romm_client.h"
#include "util.h"
#include "vita_fs.h"
#include "zip_layout.h"

int installer_copy_zip_entry(zip_t *archive, zip_uint64_t index, const char *dst);
int installer_install_content_zip(AppState *state, const char *zip_path, int platform_index, int *matched, int *skipped, char *vita_title_id, size_t vita_title_id_size, char *first_failed, size_t first_failed_size);
int installer_install_folder_zip(AppState *state, const char *zip_path, int platform_index, const RomItem *rom, int *matched, int *skipped, char *first_failed, size_t first_failed_size);

void installer_build_rom_content_url(AppState *state, const RomItem *rom, const char *file_name, char *url, size_t url_size);
void installer_build_rom_file_content_url(AppState *state, int file_id, const char *file_name, char *url, size_t url_size);
bool installer_download_rom_file_to(AppState *state, const RomItem *rom, const RomFileItem *file_item, const char *dst);

int installer_install_psx_multi_files(AppState *state, RomItem *rom, PlatformDef *p);

#endif
