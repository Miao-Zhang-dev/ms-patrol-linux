#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mq2_sensor.h"

static int read_sysfs_int(const char *path, int *out)
{
  FILE *fp = fopen(path, "r");
  if (!fp)
    return -1;
  fscanf(fp, "%d", out);
  fclose(fp);
  return 0;
}

static int read_sysfs_float(const char *path, float *out)
{
  FILE *fp = fopen(path, "r");
  if (!fp)
    return -1;
  fscanf(fp, "%f", out);
  fclose(fp);
  return 0;
}

int mq2_init(mq2_dev_t *dev)
{
  int i;
  if (read_sysfs_float(ADC_SCALE_PATH, &dev->scale) != 0)
  {
    perror("read scale fail");
    return -1;
  }
  dev->iir_out = 0;
  dev->idx = 0;
  for (i = 0; i < MEDIAN_FILTER_CNT; i++)
    dev->buf[i] = 0;
  return 0;
}

float mq2_read_raw_volt(mq2_dev_t *dev)
{
  int raw;
  if (read_sysfs_int(ADC_SYSFS_PATH, &raw) != 0)
  {
    return -1.0f;
  }
  return dev->scale * raw / 1000.0f;
}

static int cmp(const void *a, const void *b)
{
  return *(int *)a - *(int *)b;
}

float mq2_median_filter(mq2_dev_t *dev, int val)
{
  int tmp[MEDIAN_FILTER_CNT];
  dev->buf[dev->idx++] = val;
  if (dev->idx >= MEDIAN_FILTER_CNT)
    dev->idx = 0;

  memcpy(tmp, dev->buf, sizeof(tmp));
  qsort(tmp, MEDIAN_FILTER_CNT, sizeof(int), cmp);
  return (float)tmp[MEDIAN_FILTER_CNT / 2];
}

float mq2_iir_filter(mq2_dev_t *dev, float in)
{
  dev->iir_out = IIR_ALPHA * in + (1.0f - IIR_ALPHA) * dev->iir_out;
  return dev->iir_out;
}

float mq2_get_real_ao(mq2_dev_t *dev)
{
  int raw;
  read_sysfs_int(ADC_SYSFS_PATH, &raw);
  float med = mq2_median_filter(dev, raw);
  float med_v = dev->scale * med / 1000.0f;
  float filter_v = mq2_iir_filter(dev, med_v);
  float real_ao = filter_v * 2.0f;
  return real_ao;
}
