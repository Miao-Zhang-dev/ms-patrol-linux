#include "lvgl_thread.h"

#include "../config/app_config.h"
#include "../include/app_context.h"
#include "../include/app_msg.h"
#include "../common/logger.h"
#include "../common/mailbox.h"
#include "../ui/ui_update.h"

#include <errno.h>

static int handle_ui_msg(const app_msg_t * msg)
{
    if(msg == NULL) {
        return -EINVAL;
    }

    switch(msg->type) {
        case APP_MSG_SENSOR_DATA: ui_update_sensor_data(&msg->data.sensor); break;

        case APP_MSG_ALARM_EVENT: ui_update_alarm_event(&msg->data.alarm); break;

        case APP_MSG_EXIT: return 1;

        default: break;
    }

    return 0;
}

void * lvgl_thread_entry(void * arg)
{
    app_context_t * ctx = (app_context_t *)arg;

    if(ctx == NULL || ctx->ui_mailbox == NULL) {
        return NULL;
    }

    if(ui_init() != 0) {
        LOG_ERROR("ui_init failed");
        return NULL;
    }

    LOG_INFO("lvgl_thread started");

    while(ctx->running) {
        app_msg_t msg;
        int ret;

        ret = mailbox_recv_timeout(ctx->ui_mailbox, &msg, LVGL_TIMER_PERIOD_MS);
        if(ret == 0) {
            ret = handle_ui_msg(&msg);
            if(ret > 0) {
                break;
            }
        } else if(ret != -ETIMEDOUT) {
            LOG_WARN("lvgl mailbox recv failed: %d", ret);
        }

        ui_periodic();
    }

    ui_deinit();
    LOG_INFO("lvgl_thread stopped");
    return NULL;
}
