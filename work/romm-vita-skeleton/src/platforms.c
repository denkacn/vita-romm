#include "platforms.h"
#include "config.h"
#include "util.h"

static bool slug_matches(const char *slug, const char *value) {
    return strcmp(slug, value) == 0;
}

static bool slug_is_psvita(const char *slug, const char *label) {
    return slug_matches(slug, "psvita") || slug_matches(slug, "vita") || strstr(label, "vita");
}

static bool slug_is_psp(const char *slug, const char *label) {
    return slug_matches(slug, "psp") || strstr(label, "portable");
}

static bool slug_is_psx(const char *slug, const char *label) {
    return slug_matches(slug, "psx") || slug_matches(slug, "ps1") || slug_matches(slug, "playstation") ||
        strcmp(label, "playstation") == 0 || strstr(label, "playstation 1") || strstr(label, "ps1");
}

static bool slug_is_snes(const char *slug, const char *label) {
    return slug_matches(slug, "snes") || slug_matches(slug, "super-nintendo") ||
        slug_matches(slug, "super-nintendo-entertainment-system") || strstr(label, "super nintendo");
}

static void mark_known_platform(PlatformDef *p, SupportLevel support, PlatformIconIndex icon) {
    p->support_level = support;
    p->icon_index = icon;
    p->known = true;
}

void configure_platform(AppState *state, PlatformDef *p) {
    char label_l[96];
    snprintf(label_l, sizeof(label_l), "%s", p->label);
    lowercase_ascii(label_l);

    p->install_mode = INSTALL_FOLDER;
    p->support_level = SUPPORT_PATH_REQUIRED;
    p->icon_index = ICON_FALLBACK;
    p->known = false;
    p->path_configured = false;
    p->path[0] = '\0';

    if (slug_is_psvita(p->slug, label_l)) {
        snprintf(p->slug, sizeof(p->slug), "psvita");
        snprintf(p->label, sizeof(p->label), "PS Vita");
        snprintf(p->path, sizeof(p->path), "%s", state->vita_download_dir);
        p->install_mode = INSTALL_VITA;
        mark_known_platform(p, SUPPORT_MANAGED, ICON_PSVITA);
        p->path_configured = true;
        return;
    }

    if (slug_is_psp(p->slug, label_l)) {
        snprintf(p->slug, sizeof(p->slug), "psp");
        snprintf(p->label, sizeof(p->label), "PSP");
        snprintf(p->path, sizeof(p->path), "%s", state->psp_install_dir);
        p->install_mode = INSTALL_ADRENALINE_PSP;
        mark_known_platform(p, SUPPORT_MANAGED, ICON_PSP);
        p->path_configured = true;
        return;
    }

    if (slug_is_psx(p->slug, label_l)) {
        snprintf(p->slug, sizeof(p->slug), "psx");
        snprintf(p->label, sizeof(p->label), "PSX");
        p->install_mode = INSTALL_PSX;
        mark_known_platform(p, SUPPORT_VERIFIED, ICON_PSX);
    } else if (slug_matches(p->slug, "gba")) {
        mark_known_platform(p, SUPPORT_VERIFIED, ICON_GBA);
    } else if (slug_matches(p->slug, "gb") || slug_matches(p->slug, "gameboy")) {
        snprintf(p->slug, sizeof(p->slug), "gb");
        mark_known_platform(p, SUPPORT_VERIFIED, ICON_GB);
    } else if (slug_matches(p->slug, "gbc") || slug_matches(p->slug, "gameboy-color")) {
        snprintf(p->slug, sizeof(p->slug), "gbc");
        mark_known_platform(p, SUPPORT_VERIFIED, ICON_GBC);
    } else if (slug_matches(p->slug, "nes")) {
        mark_known_platform(p, SUPPORT_VERIFIED, ICON_NES);
    } else if (slug_is_snes(p->slug, label_l)) {
        snprintf(p->slug, sizeof(p->slug), "snes");
        snprintf(p->label, sizeof(p->label), "SNES");
        mark_known_platform(p, SUPPORT_VERIFIED, ICON_SNES);
    } else if (slug_matches(p->slug, "genesis") || slug_matches(p->slug, "megadrive") || slug_matches(p->slug, "mega-drive")) {
        snprintf(p->slug, sizeof(p->slug), "genesis");
        mark_known_platform(p, SUPPORT_VERIFIED, ICON_GENESIS);
    } else if (slug_matches(p->slug, "n64") || slug_matches(p->slug, "nintendo-64")) {
        snprintf(p->slug, sizeof(p->slug), "n64");
        mark_known_platform(p, SUPPORT_UNTESTED, ICON_N64);
    }

    const char *path = get_platform_path_config(state, p->slug);
    if (path[0]) {
        snprintf(p->path, sizeof(p->path), "%s", path);
        p->path_configured = true;
        if (!p->known) p->support_level = SUPPORT_UNTESTED;
    }
}
