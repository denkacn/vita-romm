#include "job_queue.h"

void job_queue_init(JobQueue *queue) {
    if (!queue) return;
    memset(queue, 0, sizeof(*queue));
}

bool job_queue_push(JobQueue *queue, const AppJob *job) {
    if (!queue || !job || queue->count >= JOB_QUEUE_CAPACITY) return false;
    queue->items[queue->tail] = *job;
    queue->tail = (queue->tail + 1) % JOB_QUEUE_CAPACITY;
    queue->count++;
    return true;
}

bool job_queue_pop(JobQueue *queue, AppJob *job) {
    if (!queue || !job || queue->count <= 0) return false;
    *job = queue->items[queue->head];
    queue->head = (queue->head + 1) % JOB_QUEUE_CAPACITY;
    queue->count--;
    return true;
}

bool job_queue_empty(const JobQueue *queue) {
    return !queue || queue->count <= 0;
}
