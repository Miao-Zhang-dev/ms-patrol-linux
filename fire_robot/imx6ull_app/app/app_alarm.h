#ifndef APP_ALARM_H
#define APP_ALARM_H

#include "../include/alarm_event.h"
#include "../include/sensor_data.h"

int app_alarm_evaluate(const sensor_data_t * data, alarm_event_t * event);
void app_alarm_make_normal_event(const sensor_data_t * data, alarm_event_t * event);

#endif
