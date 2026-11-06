#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "../include/sensor_data.h"

#ifndef SHT30_DEV_PATH
#define SHT30_DEV_PATH "/dev/robotsht30"
#endif

#ifndef BH1750_DEV_PATH
#define BH1750_DEV_PATH "/dev/robotbh1750"
#endif

#ifndef FLAME_DEV_PATH
#define FLAME_DEV_PATH "/dev/flame_sensor"
#endif

int sensor_manager_init(void);
int sensor_manager_read_all(sensor_data_t *data);
void sensor_manager_deinit(void);

#endif
