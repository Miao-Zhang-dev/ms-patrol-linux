#ifndef APP_MSG_H
#define APP_MSG_H

#include <time.h>
#include "sensor_data.h"
#include "alarm_event.h"

typedef enum {
    APP_MSG_NONE = 0,
    APP_MSG_EXIT,
    APP_MSG_SENSOR_DATA,
    APP_MSG_ALARM_EVENT,
} app_msg_type_t;

typedef struct
{
    app_msg_type_t type;
    time_t timestamp;

    union
    {
        sensor_data_t sensor;
        alarm_event_t alarm;
    } data;
} app_msg_t;

#endif