#include "../sensors/sensor_manager.h"
#include "../include/app_context.h"
#include "../include/app_msg.h"
#include "../common/mailbox.h"
#include "../common/logger.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#define SENSOR_THREAD_DEFAULT_PERIOD_MS 1000

static int get_sample_period_ms(const app_context_t *ctx)
{
  if (ctx == NULL || ctx->sensor_sample_period_ms <= 0)
  {
    return SENSOR_THREAD_DEFAULT_PERIOD_MS;
  }

  return ctx->sensor_sample_period_ms;
}

static int sensor_thread_should_run(const app_context_t *ctx)
{
  return ctx != NULL && ctx->running;
}

static void sleep_ms(int ms)
{
  if (ms <= 0)
  {
    return;
  }

  usleep((useconds_t)ms * 1000u);
}

static int post_sensor_data(mailbox_t *mailbox, const sensor_data_t *data)
{
  app_msg_t msg;

  if (mailbox == NULL)
  {
    return 0;
  }

  if (data == NULL)
  {
    return -EINVAL;
  }

  memset(&msg, 0, sizeof(msg));
  msg.type = APP_MSG_SENSOR_DATA;
  msg.timestamp = data->timestamp;
  msg.data.sensor = *data;

  return mailbox_post(mailbox, &msg);
}

static void publish_sensor_data(app_context_t *ctx, const sensor_data_t *data)
{
  int ret;

  ret = post_sensor_data(ctx->ui_mailbox, data);
  if (ret != 0)
  {
    LOG_WARN("post sensor data to ui mailbox failed: %d", ret);
  }

  ret = post_sensor_data(ctx->warning_mailbox, data);
  if (ret != 0)
  {
    LOG_WARN("post sensor data to warning mailbox failed: %d", ret);
  }

  ret = post_sensor_data(ctx->db_mailbox, data);
  if (ret != 0)
  {
    LOG_WARN("post sensor data to db mailbox failed: %d", ret);
  }

  ret = post_sensor_data(ctx->mqtt_mailbox, data);
  if (ret != 0)
  {
    LOG_WARN("post sensor data to mqtt mailbox failed: %d", ret);
  }
}

void *sensor_thread_entry(void *arg)
{
  app_context_t *ctx = (app_context_t *)arg;
  int period_ms;

  if (ctx == NULL)
  {
    return NULL;
  }

  period_ms = get_sample_period_ms(ctx);

  if (sensor_manager_init() != 0)
  {
    LOG_ERROR("sensor_manager_init failed");
    return NULL;
  }

  LOG_INFO("sensor_thread started");

  while (sensor_thread_should_run(ctx))
  {
    sensor_data_t data;
    int ret;

    ret = sensor_manager_read_all(&data);
    if (ret < 0)
    {
      LOG_WARN("sensor_manager_read_all partial failed: %d", ret);
    }

    if (data.valid_mask != 0)
    {
      publish_sensor_data(ctx, &data);
    }
    else
    {
      LOG_WARN("all sensor reads failed");
    }

    sleep_ms(period_ms);
  }

  sensor_manager_deinit();
  LOG_INFO("sensor_thread stopped");

  return NULL;
}
