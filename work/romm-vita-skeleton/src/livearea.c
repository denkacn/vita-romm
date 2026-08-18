#include "livearea.h"
#include "util.h"
#include "vita_fs.h"

static int load_sce_paf(void) {
    SceSysmoduleOpt opt;
    memset(&opt, 0, sizeof(opt));
    opt.result = &opt.flags;
    uint32_t args[] = { 0x400000, 0xEA60, 0x40000, 0, 0 };
    return sceSysmoduleLoadModuleInternalWithArg(SCE_SYSMODULE_INTERNAL_PAF, sizeof(args), args, &opt);
}

static int unload_sce_paf(void) {
    return sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_PAF);
}

static int promote_app_from_temp(const char *path) {
    int res = load_sce_paf();
    if (res < 0) {
        char log_line[160];
        snprintf(log_line, sizeof(log_line), "Promoter PAF load failed: 0x%08X path=%s", (unsigned int)res, path);
        append_install_log(log_line);
        return res;
    }

    res = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    if (res >= 0) {
        res = scePromoterUtilityInit();
        if (res >= 0) {
            res = scePromoterUtilityPromotePkgWithRif(path, 1);
            if (res < 0) {
                char log_line[180];
                snprintf(log_line, sizeof(log_line), "PromotePkgWithRif failed: 0x%08X path=%s", (unsigned int)res, path);
                append_install_log(log_line);
                res = scePromoterUtilityPromotePkg(path, 1);
                if (res < 0) {
                    snprintf(log_line, sizeof(log_line), "PromotePkg failed: 0x%08X path=%s", (unsigned int)res, path);
                    append_install_log(log_line);
                }
            }
            scePromoterUtilityExit();
        } else {
            char log_line[180];
            snprintf(log_line, sizeof(log_line), "Promoter init failed: 0x%08X path=%s", (unsigned int)res, path);
            append_install_log(log_line);
        }
        sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    } else {
        char log_line[180];
        snprintf(log_line, sizeof(log_line), "Promoter module load failed: 0x%08X path=%s", (unsigned int)res, path);
        append_install_log(log_line);
    }
    unload_sce_paf();
    return res;
}

static int copy_first_license_from_dir(const char *license_dir, const char *work_bin_path) {
    SceUID dfd = sceIoDopen(license_dir);
    if (dfd < 0) return dfd;

    int result = -1;
    while (1) {
        SceIoDirent ent;
        memset(&ent, 0, sizeof(ent));
        int res = sceIoDread(dfd, &ent);
        if (res <= 0) break;
        if (SCE_S_ISDIR(ent.d_stat.st_mode)) continue;
        if (!ends_with_ci(ent.d_name, ".rif") && !ends_with_ci(ent.d_name, ".bin")) continue;

        char rif_path[384];
        snprintf(rif_path, sizeof(rif_path), "%s/%s", license_dir, ent.d_name);
        result = copy_file(rif_path, work_bin_path);
        if (result >= 0) break;
    }

    sceIoDclose(dfd);
    return result;
}

static int copy_first_license_to_work_bin(const char *title_id, const char *work_bin_path) {
    char license_dir[192];
    snprintf(license_dir, sizeof(license_dir), "ux0:license/app/%s", title_id);
    int res = copy_first_license_from_dir(license_dir, work_bin_path);
    if (res >= 0) return res;

    snprintf(license_dir, sizeof(license_dir), "ux0:nonpdrm/license/app/%s", title_id);
    return copy_first_license_from_dir(license_dir, work_bin_path);
}

static int prepare_promoter_package(const char *app_path, const char *title_id) {
    char check_path[256];
    SceIoStat st;
    memset(&st, 0, sizeof(st));

    snprintf(check_path, sizeof(check_path), "%s/eboot.bin", app_path);
    if (sceIoGetstat(check_path, &st) < 0) return -0x101;

    snprintf(check_path, sizeof(check_path), "%s/sce_sys/param.sfo", app_path);
    if (sceIoGetstat(check_path, &st) < 0) return -0x102;

    snprintf(check_path, sizeof(check_path), "%s/sce_sys/package", app_path);
    sceIoMkdir(check_path, VITA_DIR_MODE);

    char work_bin_path[256];
    snprintf(work_bin_path, sizeof(work_bin_path), "%s/sce_sys/package/work.bin", app_path);
    if (sceIoGetstat(work_bin_path, &st) >= 0) return 0;

    int res = copy_first_license_to_work_bin(title_id, work_bin_path);
    if (res >= 0) return 0;

    char log_line[260];
    snprintf(log_line, sizeof(log_line), "No work.bin/rif for title=%s app_path=%s last=0x%08X", title_id, app_path, (unsigned int)res);
    append_install_log(log_line);
    return -0x103;
}

static int refresh_vita_livearea_for_path(const char *src, const char *temp, const char *title_id, bool needs_work_bin) {
    if (!title_id || !title_id[0]) return -1;

    sceIoMkdir("ux0:temp", VITA_DIR_MODE);
    remove_path_recursive(temp);

    int res = sceIoRename(src, temp);
    if (res < 0) return res;

    if (needs_work_bin) {
        res = prepare_promoter_package(temp, title_id);
        if (res < 0) {
            sceIoRename(temp, src);
            return res;
        }
    }

    res = promote_app_from_temp(temp);
    if (res < 0) {
        sceIoRename(temp, src);
        return res;
    }

    remove_path_recursive(temp);
    return 0;
}

int refresh_vita_livearea_for_title(const AppState *state, const char *title_id) {
    char src[192];
    snprintf(src, sizeof(src), "%s/%s", state->vita_app_dir, title_id);
    return refresh_vita_livearea_for_path(src, "ux0:temp/app", title_id, true);
}

static int refresh_vita_patch_for_title(const AppState *state, const char *title_id) {
    char src[192];
    snprintf(src, sizeof(src), "%s/%s", state->vita_patch_dir, title_id);
    return refresh_vita_livearea_for_path(src, "ux0:temp/patch", title_id, false);
}

static bool is_vita_title_dir_name(const char *name) {
    if (!name || strlen(name) != 9) return false;
    for (int i = 0; i < 9; i++) {
        char c = name[i];
        bool digit = c >= '0' && c <= '9';
        bool upper = c >= 'A' && c <= 'Z';
        if (!digit && !upper) return false;
    }
    return true;
}

static bool livearea_entry_exists(const char *title_id) {
    char path[96];
    SceIoStat st;
    memset(&st, 0, sizeof(st));
    snprintf(path, sizeof(path), "ux0:appmeta/%s/param.sfo", title_id);
    if (sceIoGetstat(path, &st) >= 0 && !SCE_S_ISDIR(st.st_mode) && st.st_size > 0) return true;
    snprintf(path, sizeof(path), "ux0:appmeta/%s", title_id);
    return sceIoGetstat(path, &st) >= 0 && SCE_S_ISDIR(st.st_mode);
}

void refresh_livearea_from_main(AppState *state) {
    char title_ids[MAX_REFRESH_TITLES][16];
    char patch_ids[MAX_REFRESH_TITLES][16];
    int title_count = 0;
    int patch_count = 0;

    state->busy = true;
    state->progress.current = 0;
    state->progress.total = 0;
    snprintf(state->status, sizeof(state->status), "Scanning Vita apps");
    snprintf(state->detail, sizeof(state->detail), "%.200s", state->vita_app_dir);
    redraw_now(state);

    SceUID dfd = sceIoDopen(state->vita_app_dir);
    if (dfd < 0) {
        snprintf(state->status, sizeof(state->status), "Refresh failed");
        snprintf(state->detail, sizeof(state->detail), "Cannot open %.180s: 0x%08X", state->vita_app_dir, (unsigned int)dfd);
        state->busy = false;
        return;
    }

    while (title_count < MAX_REFRESH_TITLES) {
        SceIoDirent ent;
        memset(&ent, 0, sizeof(ent));
        int res = sceIoDread(dfd, &ent);
        if (res <= 0) break;
        if (!SCE_S_ISDIR(ent.d_stat.st_mode)) continue;
        if (strcmp(ent.d_name, ".") == 0 || strcmp(ent.d_name, "..") == 0) continue;
        if (strcmp(ent.d_name, APP_TITLEID) == 0) continue;
        if (!is_vita_title_dir_name(ent.d_name)) continue;
        snprintf(title_ids[title_count], sizeof(title_ids[title_count]), "%s", ent.d_name);
        title_count++;
    }
    sceIoDclose(dfd);

    dfd = sceIoDopen(state->vita_patch_dir);
    if (dfd >= 0) {
        while (patch_count < MAX_REFRESH_TITLES) {
            SceIoDirent ent;
            memset(&ent, 0, sizeof(ent));
            int res = sceIoDread(dfd, &ent);
            if (res <= 0) break;
            if (!SCE_S_ISDIR(ent.d_stat.st_mode)) continue;
            if (strcmp(ent.d_name, ".") == 0 || strcmp(ent.d_name, "..") == 0) continue;
            if (!is_vita_title_dir_name(ent.d_name)) continue;
            snprintf(patch_ids[patch_count], sizeof(patch_ids[patch_count]), "%s", ent.d_name);
            patch_count++;
        }
        sceIoDclose(dfd);
    }

    if (title_count == 0 && patch_count == 0) {
        snprintf(state->status, sizeof(state->status), "Refresh LiveArea");
        snprintf(state->detail, sizeof(state->detail), "No Vita app/patch folders found");
        state->progress.current = 0;
        state->progress.total = 0;
        state->busy = false;
        return;
    }

    state->progress.total = title_count + patch_count;
    state->progress.current = 0;
    int ok = 0;
    int failed = 0;
    int last_error = 0;
    char last_failed[16] = "";
    for (int i = 0; i < title_count; i++) {
        state->progress.current = i + 1;
        snprintf(state->status, sizeof(state->status), "Refreshing LiveArea");
        snprintf(state->detail, sizeof(state->detail), "app %.9s  %d/%d  OK:%d  Fail:%d", title_ids[i], i + 1, title_count + patch_count, ok, failed);
        redraw_now(state);

        int res = 0;
        if (!livearea_entry_exists(title_ids[i])) {
            res = refresh_vita_livearea_for_title(state, title_ids[i]);
        }
        if (res >= 0) {
            ok++;
        } else {
            failed++;
            last_error = res;
            snprintf(last_failed, sizeof(last_failed), "%.9s", title_ids[i]);
            char log_line[180];
            snprintf(log_line, sizeof(log_line), "Refresh app failed: title=%s res=0x%08X", title_ids[i], (unsigned int)res);
            append_install_log(log_line);
        }
    }
    for (int i = 0; i < patch_count; i++) {
        state->progress.current = title_count + i + 1;
        snprintf(state->status, sizeof(state->status), "Refreshing LiveArea");
        snprintf(state->detail, sizeof(state->detail), "patch %.9s  %d/%d  OK:%d  Fail:%d", patch_ids[i], title_count + i + 1, title_count + patch_count, ok, failed);
        redraw_now(state);

        int res = refresh_vita_patch_for_title(state, patch_ids[i]);
        if (res >= 0) {
            ok++;
        } else {
            failed++;
            last_error = res;
            snprintf(last_failed, sizeof(last_failed), "%.9s", patch_ids[i]);
            char log_line[180];
            snprintf(log_line, sizeof(log_line), "Refresh patch failed: title=%s res=0x%08X", patch_ids[i], (unsigned int)res);
            append_install_log(log_line);
        }
    }

    if (failed == 0) {
        state->progress.current = state->progress.total;
        snprintf(state->status, sizeof(state->status), "Refresh complete");
        snprintf(state->detail, sizeof(state->detail), "LiveArea refreshed: %d / %d", ok, title_count + patch_count);
    } else {
        state->progress.current = state->progress.total;
        snprintf(state->status, sizeof(state->status), "Refresh complete with errors");
        snprintf(state->detail, sizeof(state->detail), "OK:%d Fail:%d Last:%s 0x%08X", ok, failed, last_failed, (unsigned int)last_error);
    }
    redraw_now(state);
    state->progress.current = 0;
    state->progress.total = 0;
    state->busy = false;
}

