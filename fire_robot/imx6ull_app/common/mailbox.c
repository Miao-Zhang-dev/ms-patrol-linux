#include "mailbox.h"

#include <errno.h>
#include <string.h>
#include <time.h>

static void timespec_add_ms(struct timespec *ts, int ms)
{
  if (ts == NULL || ms <= 0)
  {
    return;
  }

  ts->tv_sec += ms / 1000;
  ts->tv_nsec += (long)(ms % 1000) * 1000000L;

  if (ts->tv_nsec >= 1000000000L)
  {
    ts->tv_sec++;
    ts->tv_nsec -= 1000000000L;
  }
}

int mailbox_init(mailbox_t *mailbox)
{
  int ret;

  if (mailbox == NULL)
  {
    return -EINVAL;
  }

  memset(mailbox, 0, sizeof(*mailbox));

  ret = pthread_mutex_init(&mailbox->mutex, NULL);
  if (ret != 0)
  {
    return -ret;
  }

  ret = pthread_cond_init(&mailbox->not_empty, NULL);
  if (ret != 0)
  {
    pthread_mutex_destroy(&mailbox->mutex);
    return -ret;
  }

  return 0;
}

void mailbox_deinit(mailbox_t *mailbox)
{
  if (mailbox == NULL)
  {
    return;
  }

  pthread_cond_destroy(&mailbox->not_empty);
  pthread_mutex_destroy(&mailbox->mutex);
}

int mailbox_post(mailbox_t *mailbox, const app_msg_t *msg)
{
  if (mailbox == NULL || msg == NULL)
  {
    return -EINVAL;
  }

  pthread_mutex_lock(&mailbox->mutex);

  if (mailbox->count >= MAILBOX_MSG_MAX)
  {
    pthread_mutex_unlock(&mailbox->mutex);
    return -EAGAIN;
  }

  mailbox->queue[mailbox->write_pos] = *msg;
  mailbox->write_pos = (mailbox->write_pos + 1u) % MAILBOX_MSG_MAX;
  mailbox->count++;

  pthread_cond_signal(&mailbox->not_empty);
  pthread_mutex_unlock(&mailbox->mutex);

  return 0;
}

int mailbox_recv(mailbox_t *mailbox, app_msg_t *msg)
{
  if (mailbox == NULL || msg == NULL)
  {
    return -EINVAL;
  }

  pthread_mutex_lock(&mailbox->mutex);

  while (mailbox->count == 0u)
  {
    pthread_cond_wait(&mailbox->not_empty, &mailbox->mutex);
  }

  *msg = mailbox->queue[mailbox->read_pos];
  mailbox->read_pos = (mailbox->read_pos + 1u) % MAILBOX_MSG_MAX;
  mailbox->count--;

  pthread_mutex_unlock(&mailbox->mutex);

  return 0;
}

int mailbox_recv_timeout(mailbox_t *mailbox, app_msg_t *msg, int timeout_ms)
{
  struct timespec abstime;
  int ret = 0;

  if (mailbox == NULL || msg == NULL)
  {
    return -EINVAL;
  }

  if (timeout_ms < 0)
  {
    return mailbox_recv(mailbox, msg);
  }

  clock_gettime(CLOCK_REALTIME, &abstime);
  timespec_add_ms(&abstime, timeout_ms);

  pthread_mutex_lock(&mailbox->mutex);

  while (mailbox->count == 0u)
  {
    ret = pthread_cond_timedwait(&mailbox->not_empty, &mailbox->mutex, &abstime);
    if (ret == ETIMEDOUT)
    {
      pthread_mutex_unlock(&mailbox->mutex);
      return -ETIMEDOUT;
    }
    if (ret != 0)
    {
      pthread_mutex_unlock(&mailbox->mutex);
      return -ret;
    }
  }

  *msg = mailbox->queue[mailbox->read_pos];
  mailbox->read_pos = (mailbox->read_pos + 1u) % MAILBOX_MSG_MAX;
  mailbox->count--;

  pthread_mutex_unlock(&mailbox->mutex);

  return 0;
}
