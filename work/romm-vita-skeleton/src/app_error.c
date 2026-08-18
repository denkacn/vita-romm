#include "app_error.h"

void app_error_clear(AppError *error) {
    if (!error) return;
    memset(error, 0, sizeof(*error));
}

void app_error_set(AppError *error, AppErrorKind kind, int code, long http_status, const char *message) {
    if (!error) return;
    memset(error, 0, sizeof(*error));
    error->kind = kind;
    error->code = code;
    error->http_status = http_status;
    snprintf(error->message, sizeof(error->message), "%s", message ? message : "");
}

void app_error_apply_status(AppState *state, const AppError *error, const char *status) {
    if (!state || !error) return;
    snprintf(state->status, sizeof(state->status), "%s", status ? status : "Error");
    if (error->http_status > 0) {
        snprintf(state->detail, sizeof(state->detail), "%.140s / HTTP %ld", error->message, error->http_status);
    } else if (error->code != 0) {
        snprintf(state->detail, sizeof(state->detail), "%.140s / 0x%08X", error->message, (unsigned int)error->code);
    } else {
        snprintf(state->detail, sizeof(state->detail), "%.255s", error->message);
    }
}
