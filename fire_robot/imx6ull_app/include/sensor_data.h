#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <time.h>

typedef enum
{
  SENSOR_VALID_SHT30 = 1u << 0,
  SENSOR_VALID_BH1750 = 1u << 1,
  SENSOR_VALID_MQ2 = 1u << 2,
  SENSOR_VALID_FLAME = 1u << 3,
} sensor_valid_flag_t;

typedef struct
{
  time_t timestamp;

  float temperature;
  float humidity;
  float light_lux;

  float mq2_raw_volt;
  float mq2_real_ao_volt;

  int flame_detected;

  unsigned int valid_mask;
} sensor_data_t;

#endif
