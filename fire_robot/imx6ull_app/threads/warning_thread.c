#include "warning_thread.h"

#include "../app/app_alarm.h"
#include "../config/app_config.h"
#include "../include/app_context.h"
#include "../include/app_msg.h"
#include "../common/logger.h"
#include "../common/mailbox.h"
#include "../camera/camera_capture.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#define ALARM_IMAGE_DIR "/lvgl/LVGL-V8_3/lv_port_linux_frame_buffer/fire_robot/alarm_images"

static void ensure_alarm_image_dir(void)
{
    if(mkdir(ALARM_IMAGE_DIR, 0755) != 0 && errno != EEXIST) {
        LOG_WARN("create alarm image dir failed: %s", ALARM_IMAGE_DIR);
    }
}

static void make_alarm_image_path(char * buf, size_t size, time_t timestamp)
{
    struct tm tm_info;

    if(buf == NULL || size == 0u) {
        return;
    }

    localtime_r(&timestamp, &tm_info);
    snprintf(buf, size, ALARM_IMAGE_DIR "/alarm_%04d%02d%02d_%02d%02d%02d.ppm", tm_info.tm_year + 1900,
             tm_info.tm_mon + 1, tm_info.tm_mday, tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec);
}

static void capture_alarm_image(alarm_event_t * event)
{
    char path[160];

    if(event == NULL) {
        return;
    }

    ensure_alarm_image_dir();

    make_alarm_image_path(path, sizeof(path), event->timestamp);

    if(camera_capture_once(path) == 0) {
        snprintf(event->image_path, sizeof(event->image_path), "%s", path);
        LOG_INFO("alarm image saved: %s", event->image_path);
    } else {
        event->image_path[0] = '\0';
        LOG_WARN("alarm image capture failed");
    }
}

static int post_alarm_event(mailbox_t * mailbox, const alarm_event_t * event)
{
    app_msg_t msg;

    if(mailbox == NULL || event == NULL) {
        return 0;
    }

    memset(&msg, 0, sizeof(msg));
    msg.type       = APP_MSG_ALARM_EVENT;
    msg.timestamp  = event->timestamp;
    msg.data.alarm = *event;

    return mailbox_post(mailbox, &msg);
}

static int alarm_state_changed(const alarm_event_t * event, const alarm_event_t * last_event)
{
    if(event == NULL || last_event == NULL) {
        return 0;
    }

    return event->level != last_event->level || event->source_mask != last_event->source_mask;
}

void * warning_thread_entry(void * arg)
{
    app_context_t * ctx = (app_context_t *)arg;
    alarm_event_t last_event;
    int alarm_active = 0;

    if(ctx == NULL || ctx->warning_mailbox == NULL) {
        return NULL;
    }

    memset(&last_event, 0, sizeof(last_event));
    LOG_INFO("warning_thread started");

    while(ctx->running) {
        app_msg_t msg;
        alarm_event_t event;
        int ret;
        int is_alarm;

        ret = mailbox_recv_timeout(ctx->warning_mailbox, &msg, 200);
        if(ret == -ETIMEDOUT) {
            continue;
        }
        if(ret != 0) {
            LOG_WARN("warning mailbox recv failed: %d", ret);
            continue;
        }
        if(msg.type == APP_MSG_EXIT) {
            break;
        }
        if(msg.type != APP_MSG_SENSOR_DATA) {
            continue;
        }

        is_alarm = app_alarm_evaluate(&msg.data.sensor, &event);
        if(is_alarm) {
            if(!alarm_active) {
                capture_alarm_image(&event);
                post_alarm_event(ctx->ui_mailbox, &event);
                last_event = event;
            } else if(alarm_state_changed(&event, &last_event)) {
                capture_alarm_image(&event);

                post_alarm_event(ctx->ui_mailbox, &event);
                post_alarm_event(ctx->db_mailbox, &event);

                last_event = event;
            }

            alarm_active = 1;
        }
    }

    LOG_INFO("warning_thread stopped");
    return NULL;
}
