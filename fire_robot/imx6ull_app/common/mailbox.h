#ifndef MAILBOX_H
#define MAILBOX_H

#include "../include/app_msg.h"

#include <pthread.h>
#include <stddef.h>

#define MAILBOX_MSG_MAX 16

typedef struct
{
  pthread_mutex_t mutex;
  pthread_cond_t not_empty;
  app_msg_t queue[MAILBOX_MSG_MAX];
  size_t read_pos;
  size_t write_pos;
  size_t count;
} mailbox_t;

int mailbox_init(mailbox_t *mailbox);
void mailbox_deinit(mailbox_t *mailbox);
int mailbox_post(mailbox_t *mailbox, const app_msg_t *msg);
int mailbox_recv(mailbox_t *mailbox, app_msg_t *msg);
int mailbox_recv_timeout(mailbox_t *mailbox, app_msg_t *msg, int timeout_ms);

#endif
