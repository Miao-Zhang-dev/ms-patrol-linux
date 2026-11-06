#ifndef APP_CONTEXT_H
#define APP_CONTEXT_H

#include "../common/mailbox.h"

typedef struct
{
  volatile int running;
  int sensor_sample_period_ms;

  mailbox_t *ui_mailbox;
  mailbox_t *warning_mailbox;
  mailbox_t *db_mailbox;
  mailbox_t *mqtt_mailbox;
} app_context_t;

#endif
