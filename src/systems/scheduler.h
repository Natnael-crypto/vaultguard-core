#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_SCHEDULED_ENTITIES 256

typedef struct {
    uint32_t entity_id;
    int energy;
    int speed;
    int tick_offset;
} SchedulerEntry;

typedef struct {
    SchedulerEntry entries[MAX_SCHEDULED_ENTITIES];
    int count;
    int current_index;
    uint32_t tick;
    bool round_robin;
} Scheduler;

void scheduler_init(Scheduler *s);
int scheduler_add(Scheduler *s, uint32_t entity_id, int speed);
int scheduler_remove(Scheduler *s, uint32_t entity_id);
int scheduler_next(Scheduler *s);
void scheduler_reset(Scheduler *s);
void scheduler_set_speed(Scheduler *s, uint32_t entity_id, int speed);
uint32_t scheduler_get_current(const Scheduler *s);
bool scheduler_has_entity(const Scheduler *s, uint32_t entity_id);
void scheduler_clear(Scheduler *s);

#endif
