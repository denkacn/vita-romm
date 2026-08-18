#include "json_utils.h"
#include "util.h"

int parse_platform_defs(const char *json, PlatformDef *platforms, int max_platforms) {
    int count = 0;
    const char *scan_start = json;
    const char *scan_end = json + strlen(json);
    const char *array_start = NULL;
    const char *array_end = NULL;
    if (find_json_array_field_partial(json, "items", &array_start, &array_end) ||
        find_json_array_field_partial(json, "results", &array_start, &array_end) ||
        find_json_array_field_partial(json, "data", &array_start, &array_end)) {
        scan_start = array_start;
        scan_end = array_end;
    } else {
        const char *first_array = strchr(json, '[');
        if (first_array) {
            const char *first_array_end = find_json_array_end(first_array);
            if (first_array_end) {
                scan_start = first_array;
                scan_end = first_array_end;
            }
        }
    }

    const char *cur = scan_start;
    while (count < max_platforms && cur < scan_end && (cur = strchr(cur, '{')) != NULL && cur < scan_end) {
        const char *end = find_json_object_end(cur);
        if (!end || end > scan_end) break;
        int id = -1;
        char name[96] = "";
        char slug[96] = "";
        char folder[96] = "";
        extract_int(cur, end, "id", &id);
        extract_string(cur, end, "name", name, sizeof(name));
        extract_string(cur, end, "slug", slug, sizeof(slug));
        extract_string(cur, end, "fs_slug", folder, sizeof(folder));
        if (id < 0 || (!name[0] && !slug[0] && !folder[0])) {
            cur = end + 1;
            continue;
        }

        PlatformDef *p = &platforms[count];
        memset(p, 0, sizeof(*p));
        p->id = id;
        snprintf(p->label, sizeof(p->label), "%s", name[0] ? name : (slug[0] ? slug : folder));
        snprintf(p->slug, sizeof(p->slug), "%s", slug[0] ? slug : (folder[0] ? folder : name));
        lowercase_ascii(p->slug);
        sanitize_filename(p->slug);
        p->ids[0] = id;
        p->id_count = 1;
        count++;
        cur = end + 1;
    }
    return count;
}
