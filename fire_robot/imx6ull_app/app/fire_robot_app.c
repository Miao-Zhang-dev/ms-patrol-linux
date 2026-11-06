#include "fire_robot_app.h"

#include "../config/app_config.h"
#include "../include/app_context.h"
#include "../include/app_msg.h"
#include "../common/logger.h"
#include "../common/mailbox.h"
#include "../threads/sensor_thread.h"
#include "../threads/warning_thread.h"
#include "../threads/db_thread.h"
#include "../ui/ui_update.h"

#include <errno.h>
#include <pthread.h>
#include <string.h>

static app_context_t g_app;
static mailbox_t g_ui_mailbox;
static mailbox_t g_warning_mailbox;
static mailbox_t g_db_mailbox;

static pthread_t g_sensor_tid;
static pthread_t g_warning_tid;
static pthread_t g_db_tid;

static int g_inited;
static int g_sensor_started;
static int g_warning_started;
static int g_db_started;

static void post_exit_msg(mailbox_t * mailbox)
{
    app_msg_t msg;

    if(mailbox == NULL) {
        return;
    }

    memset(&msg, 0, sizeof(msg));
    msg.type = APP_MSG_EXIT;
    mailbox_post(mailbox, &msg);
}

static void handle_ui_msg(const app_msg_t * msg)
{
    if(msg == NULL) {
        return;
    }

    switch(msg->type) {
        case APP_MSG_SENSOR_DATA: ui_update_sensor_data(&msg->data.sensor); break;

        case APP_MSG_ALARM_EVENT: ui_update_alarm_event(&msg->data.alarm); break;

        case APP_MSG_EXIT: fire_robot_app_request_stop(); break;

        default: break;
    }
}

int fire_robot_app_init(void)
{
    if(g_inited) {
        return 0;
    }

    memset(&g_app, 0, sizeof(g_app));

    if(mailbox_init(&g_ui_mailbox) != 0) {
        return -1;
    }

    if(mailbox_init(&g_warning_mailbox) != 0) {
        mailbox_deinit(&g_ui_mailbox);
        return -1;
    }

    if(mailbox_init(&g_db_mailbox) != 0) {
        mailbox_deinit(&g_warning_mailbox);
        mailbox_deinit(&g_ui_mailbox);
        return -1;
    }

    g_app.running                 = 1;
    g_app.sensor_sample_period_ms = SENSOR_SAMPLE_PERIOD_MS;
    g_app.ui_mailbox              = &g_ui_mailbox;
    g_app.warning_mailbox         = &g_warning_mailbox;
    g_app.db_mailbox              = &g_db_mailbox;
    g_app.mqtt_mailbox            = NULL;

    g_inited = 1;
    return 0;
}

int fire_robot_app_start(void)
{
    if(!g_inited) {
        return -1;
    }

    if(pthread_create(&g_sensor_tid, NULL, sensor_thread_entry, &g_app) != 0) {
        LOG_ERROR("create sensor thread failed");
        return -1;
    }
    g_sensor_started = 1;

    if(pthread_create(&g_warning_tid, NULL, warning_thread_entry, &g_app) != 0) {
        LOG_ERROR("create warning thread failed");
        fire_robot_app_request_stop();
        pthread_join(g_sensor_tid, NULL);
        g_sensor_started = 0;
        return -1;
    }
    g_warning_started = 1;

    if(pthread_create(&g_db_tid, NULL, db_thread_entry, &g_app) != 0) {
        LOG_ERROR("create db thread failed");
        fire_robot_app_request_stop();

        if(g_warning_started) {
            pthread_join(g_warning_tid, NULL);
            g_warning_started = 0;
        }

        if(g_sensor_started) {
            pthread_join(g_sensor_tid, NULL);
            g_sensor_started = 0;
        }

        return -1;
    }
    g_db_started = 1;

    return 0;
}

void fire_robot_app_poll_ui(void)
{
    app_msg_t msg;
    int ret;

    if(!g_inited) {
        return;
    }

    while(1) {
        ret = mailbox_recv_timeout(&g_ui_mailbox, &msg, 0);
        if(ret == -ETIMEDOUT) {
            break;
        }
        if(ret != 0) {
            LOG_WARN("ui mailbox recv failed: %d", ret);
            break;
        }

        handle_ui_msg(&msg);
    }
}

int fire_robot_app_is_running(void)
{
    return g_inited && g_app.running;
}

void fire_robot_app_request_stop(void)
{
    if(!g_inited) {
        return;
    }

    g_app.running = 0;
    post_exit_msg(g_app.warning_mailbox);
    post_exit_msg(g_app.db_mailbox);
}

void fire_robot_app_stop(void)
{
    if(!g_inited) {
        return;
    }

    fire_robot_app_request_stop();

    if(g_sensor_started) {
        pthread_join(g_sensor_tid, NULL);
        g_sensor_started = 0;
    }

    if(g_warning_started) {
        pthread_join(g_warning_tid, NULL);
        g_warning_started = 0;
    }

    if(g_db_started) {
        pthread_join(g_db_tid, NULL);
        g_db_started = 0;
    }
}

void fire_robot_app_deinit(void)
{
    if(!g_inited) {
        return;
    }

    fire_robot_app_stop();

    mailbox_deinit(&g_db_mailbox);
    mailbox_deinit(&g_warning_mailbox);
    mailbox_deinit(&g_ui_mailbox);

    g_inited = 0;
}