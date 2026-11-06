#ifndef UI_UPDATE_H
#define UI_UPDATE_H

#include "../include/alarm_event.h"
#include "../include/sensor_data.h"

int ui_init(void);
void ui_deinit(void);
void ui_periodic(void);

void ui_alarm_log_init(void);
void ui_update_sensor_data(const sensor_data_t * data);
void ui_update_alarm_event(const alarm_event_t * event);

#endif