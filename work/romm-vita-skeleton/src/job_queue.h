#ifndef VITA_ROMM_JOB_QUEUE_H
#define VITA_ROMM_JOB_QUEUE_H

#include "app_types.h"

#define JOB_QUEUE_CAPACITY 16

typedef enum {
    JOB_NONE = 0,
    JOB_LIBRARY_PAGE,
    JOB_COVER,
    JOB_DOWNLOAD,
    JOB_INSTALL,
    JOB_LIVEAREA_REFRESH
} JobType;

typedef struct {
    JobType type;
    int generation;
    int platform_index;
    int rom_index;
    DownloadJob download;
    CoverJob cover;
} AppJob;

typedef struct {
    AppJob items[JOB_QUEUE_CAPACITY];
    int head;
    int tail;
    int count;
} JobQueue;

void job_queue_init(JobQueue *queue);
bool job_queue_push(JobQueue *queue, const AppJob *job);
bool job_queue_pop(JobQueue *queue, AppJob *job);
bool job_queue_empty(const JobQueue *queue);

#endif
