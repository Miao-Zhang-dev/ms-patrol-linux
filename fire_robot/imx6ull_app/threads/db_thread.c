#include "db_thread.h"

#include "../config/app_config.h"
#include "../include/app_context.h"
#include "../include/app_msg.h"
#include "../storage/db.h"
#include "../common/logger.h"
#include "../common/mailbox.h"

#include <errno.h>
#include <time.h>

static void handle_sensor_data(const sensor_data_t * data, time_t * last_save_time)
{
    time_t now;

    if(data == NULL || last_save_time == NULL) {
        return;
    }

    now = time(NULL);
    if(*last_save_time != 0 && now - *last_save_time < DB_SENSOR_SAVE_INTERVAL_S) {
        return;
    }

    if(db_insert_sensor_data(data) == 0) {
        *last_save_time = now;
    } else {
        LOG_WARN("insert sensor data failed");
    }
}

static void handle_alarm_event(const alarm_event_t * event)
{
    if(event == NULL || event->level == ALARM_LEVEL_NORMAL) {
        return;
    }

    if(db_insert_alarm_event(event) != 0) {
        LOG_WARN("insert alarm event failed");
    }
}

void * db_thread_entry(void * arg)
{
    app_context_t * ctx          = (app_context_t *)arg;
    time_t last_sensor_save_time = 0;

    if(ctx == NULL || ctx->db_mailbox == NULL) {
        return NULL;
    }

    if(db_open(DB_PATH) != 0) {
        LOG_ERROR("db open failed");
        return NULL;
    }

    if(db_init_schema() != 0) {
        LOG_ERROR("db init schema failed");
        db_close();
        return NULL;
    }

    LOG_INFO("db_thread started");

    while(ctx->running) {
        app_msg_t msg;
        int ret;

        ret = mailbox_recv_timeout(ctx->db_mailbox, &msg, 200);
        if(ret == -ETIMEDOUT) {
            continue;
        }
        if(ret != 0) {
            LOG_WARN("db mailbox recv failed: %d", ret);
            continue;
        }
        if(msg.type == APP_MSG_EXIT) {
            break;
        }

        if(msg.type == APP_MSG_SENSOR_DATA) {
            handle_sensor_data(&msg.data.sensor, &last_sensor_save_time);
        } else if(msg.type == APP_MSG_ALARM_EVENT) {
            handle_alarm_event(&msg.data.alarm);
        }
    }

    db_close();
    LOG_INFO("db_thread stopped");
    return NULL;
}
