#include "app_alarm.h"

#include "../config/app_config.h"

#include <stdio.h>
#include <string.h>

static void append_reason(char * buf, size_t size, const char * text)
{
    size_t len;

    if(buf == NULL || text == NULL || size == 0u) {
        return;
    }

    len = strlen(buf);
    if(len != 0u && len + 2u < size) {
        snprintf(buf + len, size - len, ", ");
        len = strlen(buf);
    }

    if(len < size) {
        snprintf(buf + len, size - len, "%s", text);
    }
}

static void init_event_from_data(const sensor_data_t * data, alarm_event_t * event)
{
    memset(event, 0, sizeof(*event));
    event->timestamp = data != NULL ? data->timestamp : 0;
    if(data != NULL) {
        event->snapshot = *data;
    }
}

int app_alarm_evaluate(const sensor_data_t * data, alarm_event_t * event)
{
    if(data == NULL || event == NULL) {
        return 0;
    }

    init_event_from_data(data, event);
    event->level = ALARM_LEVEL_NORMAL;

    if((data->valid_mask & SENSOR_VALID_FLAME) && data->flame_detected) {
        event->level = ALARM_LEVEL_DANGER;
        event->source_mask |= ALARM_SOURCE_FLAME;
        append_reason(event->reason, sizeof(event->reason), "Flame Detected");
    }

    if(data->valid_mask & SENSOR_VALID_MQ2) {
        if(data->mq2_real_ao_volt >= ALARM_MQ2_DANGER_VOLT) {
            event->level = ALARM_LEVEL_DANGER;
            event->source_mask |= ALARM_SOURCE_SMOKE;
            append_reason(event->reason, sizeof(event->reason), "Smoke Danger");
        } else if(data->mq2_real_ao_volt >= ALARM_MQ2_WARN_VOLT) {
            if(event->level < ALARM_LEVEL_WARN) {
                event->level = ALARM_LEVEL_WARN;
            }
            event->source_mask |= ALARM_SOURCE_SMOKE;
            append_reason(event->reason, sizeof(event->reason), "Smoke Warning");
        }
    }

    if(data->valid_mask & SENSOR_VALID_SHT30) {
        if(data->temperature >= ALARM_TEMP_DANGER_C) {
            event->level = ALARM_LEVEL_DANGER;
            event->source_mask |= ALARM_SOURCE_TEMP;
            append_reason(event->reason, sizeof(event->reason), "Temp Danger");
        } else if(data->temperature >= ALARM_TEMP_WARN_C) {
            if(event->level < ALARM_LEVEL_WARN) {
                event->level = ALARM_LEVEL_WARN;
            }
            event->source_mask |= ALARM_SOURCE_TEMP;
            append_reason(event->reason, sizeof(event->reason), "Temp Warning");
        }
    }

    if(event->level == ALARM_LEVEL_NORMAL) {
        return 0;
    }

    if(event->reason[0] == '\0') {
        snprintf(event->reason, sizeof(event->reason), "Fire Risk Warning");
    }

    return 1;
}

void app_alarm_make_normal_event(const sensor_data_t * data, alarm_event_t * event)
{
    if(event == NULL) {
        return;
    }

    init_event_from_data(data, event);
    event->level       = ALARM_LEVEL_NORMAL;
    event->source_mask = 0;
    snprintf(event->reason, sizeof(event->reason), "Normal");
}
