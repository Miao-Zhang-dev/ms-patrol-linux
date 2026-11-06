#ifndef DB_H
#define DB_H

#include "../include/alarm_event.h"
#include "../include/sensor_data.h"

int db_open(const char * path);
int db_init_schema(void);
int db_insert_sensor_data(const sensor_data_t * data);
int db_insert_alarm_event(const alarm_event_t * event);
void db_close(void);

#endif