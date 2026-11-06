#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include "lv_drivers/display/fbdev.h"
#include "lv_drivers/indev/evdev.h"

#include <signal.h>
#include <stdint.h>
#include <sys/time.h>
#include <unistd.h>

#include <gui/generated/gui_guider.h>
#include "lvgl/src/core/lv_obj.h"

#include "fire_robot_app.h"

#define DISP_BUF_SIZE (128 * 1024)

lv_ui guider_ui;

static volatile sig_atomic_t g_exit_flag = 0;

static void signal_handler(int signo)
{
    (void)signo;
    g_exit_flag = 1;
}

int main(void)
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    lv_init();

    fbdev_init();

    static lv_color_t buf[DISP_BUF_SIZE];

    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, DISP_BUF_SIZE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.hor_res  = 800;
    disp_drv.ver_res  = 480;
    lv_disp_drv_register(&disp_drv);

    evdev_init();

    static lv_indev_drv_t indev_drv_1;
    lv_indev_drv_init(&indev_drv_1);
    indev_drv_1.type    = LV_INDEV_TYPE_POINTER;
    indev_drv_1.read_cb = evdev_read;

    lv_indev_t * mouse_indev = lv_indev_drv_register(&indev_drv_1);

    LV_IMG_DECLARE(mouse_cursor_icon)
    lv_obj_t * cursor_obj = lv_img_create(lv_scr_act());
    lv_img_set_src(cursor_obj, &mouse_cursor_icon);
    lv_indev_set_cursor(mouse_indev, cursor_obj);

    setup_ui(&guider_ui);

    lv_obj_clear_flag(guider_ui.screen_img_1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(guider_ui.screen_datetext_1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(guider_ui.screen_digital_clock_1, LV_OBJ_FLAG_CLICKABLE);

    if(fire_robot_app_init() != 0) {
        return -1;
    }

    if(fire_robot_app_start() != 0) {
        fire_robot_app_deinit();
        return -1;
    }

    while(!g_exit_flag && fire_robot_app_is_running()) {
        fire_robot_app_poll_ui();
        lv_timer_handler();
        usleep(5000);
    }

    fire_robot_app_stop();
    fire_robot_app_deinit();

    return 0;
}

uint32_t custom_tick_get(void)
{
    static uint64_t start_ms = 0;

    if(start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);

    uint64_t now_ms;
    now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

    return (uint32_t)(now_ms - start_ms);
}