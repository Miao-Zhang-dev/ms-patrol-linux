#ifndef __MQ2_H__
#define __MQ2_H__

#define ADC_SYSFS_PATH "/sys/bus/iio/devices/iio:device0/in_voltage1_raw"
#define ADC_SCALE_PATH "/sys/bus/iio/devices/iio:device0/in_voltage_scale"

#define MEDIAN_FILTER_CNT 5
#define IIR_ALPHA 0.2f

typedef struct
{
  float scale;
  float iir_out;
  int buf[MEDIAN_FILTER_CNT];
  int idx;
} mq2_dev_t;

extern int mq2_init(mq2_dev_t *dev);
extern float mq2_read_raw_volt(mq2_dev_t *dev);
extern float mq2_get_real_ao(mq2_dev_t *dev);

#endif