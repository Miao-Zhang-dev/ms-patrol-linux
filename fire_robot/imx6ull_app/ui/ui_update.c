#include "ui_update.h"
#include "/home/linux/nfs/rootfs/lvgl/LVGL-V8_3/lv_port_linux_frame_buffer/gui/generated/events_init.h"
#include <stdio.h>
#include <time.h>

#include "/home/linux/nfs/rootfs/lvgl/LVGL-V8_3/lv_port_linux_frame_buffer/lvgl/lvgl.h"
#include "/home/linux/nfs/rootfs/lvgl/LVGL-V8_3/lv_port_linux_frame_buffer/gui/generated/gui_guider.h"
#include "/home/linux/nfs/rootfs/lvgl/LVGL-V8_3/lv_port_linux_frame_buffer/gui/custom/custom.h"

#include "../include/alarm_event.h"

LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_20);

#ifndef WARNING_LOG_PARENT
#define WARNING_LOG_PARENT guider_ui.screen_tabview_1_tab_3
#endif

#define ALARM_LOG_MAX_ITEM 20

static lv_obj_t * g_alarm_log_box;
static unsigned int g_alarm_log_count;

int ui_init(void)
{
    setup_ui(&guider_ui);
    events_init(&guider_ui);
    custom_init(&guider_ui);
    ui_alarm_log_init();
    return 0;
}

void ui_deinit(void)
{}

void ui_periodic(void)
{
    lv_timer_handler();
}

static void set_label_text(lv_obj_t * label, const char * text)
{
    if(label != NULL && text != NULL) {
        lv_obj_set_style_text_font(label, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(label, text);
    }
}

static void set_label_text_fixed1(lv_obj_t * label, float value, const char * unit)
{
    char text[32];

    if(label == NULL || unit == NULL) {
        return;
    }

    snprintf(text, sizeof(text), "%.1f %s", value, unit);
    set_label_text(label, text);
}

static void set_label_text_fixed3(lv_obj_t * label, float value, const char * unit)
{
    char text[32];

    if(label == NULL || unit == NULL) {
        return;
    }

    snprintf(text, sizeof(text), "%.3f %s", value, unit);
    set_label_text(label, text);
}

static const char * alarm_level_text(alarm_level_t level)
{
    if(level == ALARM_LEVEL_DANGER) {
        return "DANGER";
    }

    if(level == ALARM_LEVEL_WARN) {
        return "WARNING";
    }

    return "NORMAL";
}

static void format_time_text(time_t timestamp, char * buf, size_t size)
{
    struct tm tm_info;

    if(buf == NULL || size == 0u) {
        return;
    }

    localtime_r(&timestamp, &tm_info);
    snprintf(buf, size, "%04d-%02d-%02d %02d:%02d:%02d", tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday,
             tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec);
}

void ui_alarm_log_init(void)
{
    if(g_alarm_log_box != NULL) {
        return;
    }

    if(WARNING_LOG_PARENT == NULL) {
        return;
    }

    g_alarm_log_box = lv_obj_create(WARNING_LOG_PARENT);
    lv_obj_set_size(g_alarm_log_box, lv_pct(96), lv_pct(82));
    lv_obj_align(g_alarm_log_box, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_scroll_dir(g_alarm_log_box, LV_DIR_VER);
    lv_obj_set_flex_flow(g_alarm_log_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_alarm_log_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(g_alarm_log_box, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_row(g_alarm_log_box, 8, LV_PART_MAIN);
    lv_obj_set_style_radius(g_alarm_log_box, 6, LV_PART_MAIN);
    lv_obj_set_style_text_font(g_alarm_log_box, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void ui_add_alarm_log(const alarm_event_t * event)
{
    lv_obj_t * msgbox;
    char text[192];
    char time_text[32];
    const char * title;

    if(event == NULL || event->level == ALARM_LEVEL_NORMAL) {
        return;
    }

    ui_alarm_log_init();
    if(g_alarm_log_box == NULL) {
        return;
    }

    format_time_text(event->timestamp, time_text, sizeof(time_text));
    title = alarm_level_text(event->level);

    snprintf(text, sizeof(text), "%s\n%s", time_text, event->reason);

    msgbox = lv_msgbox_create(g_alarm_log_box, title, text, NULL, false);
    lv_obj_set_width(msgbox, lv_pct(100));
    lv_obj_set_style_text_font(msgbox, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(msgbox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_scroll_to_view(msgbox, LV_ANIM_ON);

    g_alarm_log_count++;
    if(g_alarm_log_count > ALARM_LOG_MAX_ITEM) {
        lv_obj_t * first = lv_obj_get_child(g_alarm_log_box, 0);
        if(first != NULL) {
            lv_obj_del(first);
            g_alarm_log_count--;
        }
    }
}

void ui_update_alarm_event(const alarm_event_t * event)
{
    if(event == NULL) {
        return;
    }

    if(event->level == ALARM_LEVEL_NORMAL) {
        set_label_text(guider_ui.screen_runstatus, "RUNNING");
        set_label_text(guider_ui.screen_error, "NORMAL");
        return;
    }

    set_label_text(guider_ui.screen_runstatus, "ALARM");
    set_label_text(guider_ui.screen_error, event->reason);

    ui_add_alarm_log(event);
}

void ui_update_sensor_data(const sensor_data_t * data)
{
    if(data == NULL) {
        return;
    }

    if(data->valid_mask & SENSOR_VALID_SHT30) {
        set_label_text_fixed1(guider_ui.screen_temp, data->temperature, "C");
        set_label_text_fixed1(guider_ui.screen_humi, data->humidity, "%");
    } else {
        set_label_text(guider_ui.screen_temp, "-- C");
        set_label_text(guider_ui.screen_humi, "-- %");
    }

    if(data->valid_mask & SENSOR_VALID_BH1750) {
        set_label_text_fixed1(guider_ui.screen_light, data->light_lux, "lx");
    } else {
        set_label_text(guider_ui.screen_light, "-- lx");
    }

    if(data->valid_mask & SENSOR_VALID_MQ2) {
        set_label_text_fixed3(guider_ui.screen_smoke, data->mq2_real_ao_volt, "V");
    } else {
        set_label_text(guider_ui.screen_smoke, "-- V");
    }

    if(data->valid_mask & SENSOR_VALID_FLAME) {
        set_label_text(guider_ui.screen_flame, data->flame_detected ? "FIRE" : "NORMAL");
    } else {
        set_label_text(guider_ui.screen_flame, "N/A");
    }

    set_label_text(guider_ui.screen_uploadtime, "NOW");
}