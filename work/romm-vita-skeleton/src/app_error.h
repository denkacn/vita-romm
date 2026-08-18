#ifndef VITA_ROMM_APP_ERROR_H
#define VITA_ROMM_APP_ERROR_H

#include "app_types.h"

typedef enum {
    APP_ERROR_NONE = 0,
    APP_ERROR_ROMM,
    APP_ERROR_NETWORK,
    APP_ERROR_DOWNLOAD,
    APP_ERROR_INSTALL,
    APP_ERROR_FILESYSTEM,
    APP_ERROR_LIVEAREA
} AppErrorKind;

typedef struct {
    AppErrorKind kind;
    int code;
    long http_status;
    char message[160];
} AppError;

void app_error_clear(AppError *error);
void app_error_set(AppError *error, AppErrorKind kind, int code, long http_status, const char *message);
void app_error_apply_status(AppState *state, const AppError *error, const char *status);

#endif
