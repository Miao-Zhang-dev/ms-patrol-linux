#ifndef ALARM_EVENT_H
#define ALARM_EVENT_H

#include "sensor_data.h"

#include <time.h>

typedef enum {
    ALARM_LEVEL_NORMAL = 0,
    ALARM_LEVEL_WARN,
    ALARM_LEVEL_DANGER,
} alarm_level_t;

typedef enum {
    ALARM_SOURCE_TEMP  = 1u << 0,
    ALARM_SOURCE_SMOKE = 1u << 1,
    ALARM_SOURCE_FLAME = 1u << 2,
} alarm_source_t;

typedef struct
{
    time_t timestamp;
    alarm_level_t level;
    unsigned int source_mask;
    char reason[128];
    char image_path[160];
    sensor_data_t snapshot;
} alarm_event_t;

#endif