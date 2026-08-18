#ifndef VITA_ROMM_ZIP_LAYOUT_H
#define VITA_ROMM_ZIP_LAYOUT_H

#include "app_types.h"

bool find_vita_zip_hint(zip_t *archive, VitaZipHint *hint);
bool find_psp_zip_hint(zip_t *archive, PspZipHint *hint);
bool get_vita_title_id_for_entry(const char *entry_name, const VitaZipHint *hint, char *dst, size_t dst_size);
void make_folder_name_from_file(const char *file, char *dst, size_t dst_size);
bool map_zip_entry_to_install_path(const AppState *state, int platform_index, const char *entry_name, const VitaZipHint *hint, const PspZipHint *psp_hint, char *dst, size_t dst_size);

#endif
