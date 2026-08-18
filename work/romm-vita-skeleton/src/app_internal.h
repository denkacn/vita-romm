#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/promoterutil.h>
#include <psp2/sysmodule.h>

#include <curl/curl.h>
#include <vita2d.h>
#include <zip.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "font_atlas.h"

#define APP_TITLE "Vita RomM"
#define APP_TITLEID "ROMM00001"
#ifndef ROMM_VITA_VERSION
#define ROMM_VITA_VERSION "0.5.18"
#endif
#define CONFIG_DIR "ux0:data/vita-romm"
#define CONFIG_PATH CONFIG_DIR "/config.txt"
#define POSTER_CACHE_DIR CONFIG_DIR "/covers"
#define PLATFORM_ICON_CACHE_DIR CONFIG_DIR "/platform-icons"
#define DOWNLOAD_DIR CONFIG_DIR "/downloads"
#define FONT_RENDERER_NAME "Inter bitmap atlas"
#define HTTP_RESPONSE_MAX 524287
#define MAX_ROMS 1024
#define MAX_PLATFORMS 32
#define MAX_PLATFORM_IDS 8
#define MAX_PLATFORM_PATHS 32
#define BUILTIN_PLATFORM_ICON_COUNT 11
#define MAX_REFRESH_TITLES 1024

#define COLOR_BG RGBA8(17, 20, 28, 255)
#define COLOR_BAR RGBA8(28, 35, 46, 255)
#define COLOR_CARD RGBA8(36, 45, 58, 255)
#define COLOR_CARD_ACTIVE RGBA8(58, 95, 132, 255)
#define COLOR_ACCENT RGBA8(91, 192, 190, 255)
#define COLOR_TEXT RGBA8(238, 242, 246, 255)
#define COLOR_MUTED RGBA8(156, 169, 184, 255)
#define COLOR_GOOD RGBA8(105, 214, 132, 255)
#define COLOR_WARN RGBA8(245, 174, 79, 255)
#define UI_FONT_SCALE 1.0f
#define UI_FONT_BASE_SIZE 32.0f
#define VITA_FILE_MODE (SCE_S_IRUSR | SCE_S_IWUSR | SCE_S_IRSYS | SCE_S_IWSYS)
#define VITA_DIR_MODE (SCE_S_IRWXU | SCE_S_IRWXS)

typedef enum {
    VIEW_LOADING,
    VIEW_PLATFORMS,
    VIEW_GAMES,
    VIEW_GAME_LIST
} ViewMode;

typedef struct {
    char label[96];
    char slug[40];
    char path[160];
    char icon_path[128];
    int id;
    int ids[MAX_PLATFORM_IDS];
    int id_count;
    int rom_count;
    int install_mode;
    int support_level;
    int icon_index;
    bool path_configured;
    bool known;
    vita2d_texture *icon;
} PlatformDef;

typedef struct {
    char slug[40];
    char path[160];
} PlatformPathConfig;

typedef enum {
    INSTALL_VITA = 0,
    INSTALL_ADRENALINE_PSP,
    INSTALL_PSX,
    INSTALL_FOLDER
} InstallMode;

typedef enum {
    SUPPORT_MANAGED = 0,
    SUPPORT_VERIFIED,
    SUPPORT_UNTESTED,
    SUPPORT_PATH_REQUIRED
} SupportLevel;

typedef enum {
    ICON_PSVITA = 0,
    ICON_PSP,
    ICON_PSX,
    ICON_GBA,
    ICON_GB,
    ICON_GBC,
    ICON_NES,
    ICON_SNES,
    ICON_GENESIS,
    ICON_N64,
    ICON_FALLBACK
} PlatformIconIndex;

typedef struct {
    char name[320];
    char url[360];
    long long size_bytes;
} RomFileItem;

typedef struct {
    int id;
    char name[160];
    char file_name[160];
    char cover_url[320];
    char cover_path[128];
    long long size_bytes;
    vita2d_texture *cover;
    bool is_downloaded;
    bool is_installed;
} RomItem;

typedef struct {
    char server_url[256];
    char api_token[256];
    char vita_install_dir[160];
    char vita_download_dir[160];
    char vita_app_dir[80];
    char vita_patch_dir[80];
    char vita_repatch_dir[80];
    char vita_addcont_dir[80];
    char vita_license_dir[80];
    char psp_install_dir[160];
    char psp_game_dir[160];
    char psp_download_dir[160];
    char psx_pbp_dir[160];
    PlatformPathConfig platform_paths[MAX_PLATFORM_PATHS];
    int platform_path_count;
    char status[256];
    char detail[256];
    char network_status[96];
    char ip_address[64];
    PlatformDef platforms[MAX_PLATFORMS];
    int platform_count;
    RomItem roms[MAX_ROMS];
    int rom_count;
    int selected_platform;
    int selected_rom;
    int progress_current;
    int progress_total;
    ViewMode view;
    bool auto_refresh_livearea;
    bool busy;
} AppState;

typedef struct {
    char root[180];
    char title_id[16];
} VitaZipHint;

typedef struct {
    char root[180];
    char folder[80];
} PspZipHint;

static char net_memory[1 * 1024 * 1024];
static char http_response[HTTP_RESPONSE_MAX + 1];
typedef struct {
    vita2d_texture *atlas;
} BitmapFont;

static BitmapFont *g_font = NULL;
static vita2d_texture *g_logo_texture = NULL;
static vita2d_texture *g_platform_textures[BUILTIN_PLATFORM_ICON_COUNT] = { NULL };

static void draw_ui(BitmapFont *font, const AppState *state);
static void update_rom_status(AppState *state, int index);

typedef struct {
    SceUID fd;
    AppState *state;
    curl_off_t last_drawn;
} DownloadContext;















