#include "camera_capture.h"

#include "video.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CAMERA_DEV_PATH "/dev/video1"
#define CAMERA_WARMUP_DELAY_MS 500
#define CAMERA_WARMUP_DROP_FRAMES 5

static int save_rgb_to_ppm(const char * path, const rgb_t * rgb, int width, int height)
{
    FILE * fp;
    int i;

    if(path == NULL || rgb == NULL) {
        return -1;
    }

    fp = fopen(path, "wb");
    if(fp == NULL) {
        return -1;
    }

    fprintf(fp, "P6\n%d %d\n255\n", width, height);

    for(i = 0; i < width * height; i++) {
        fputc(rgb[i].R, fp);
        fputc(rgb[i].G, fp);
        fputc(rgb[i].B, fp);
    }

    fclose(fp);
    return 0;
}

int camera_capture_once(const char * save_path)
{
    camera_t * camera          = NULL;
    unsigned char * yuyv_frame = NULL;
    rgb_t * rgb_frame          = NULL;
    int ret                    = -1;

    camera = camera_init(CAMERA_DEV_PATH);
    if(camera == NULL) {
        return -1;
    }

    if(camera_opendev(camera) != 0) {
        goto out;
    }

    if(camera_setfmt(camera, VIDEO_WIDTH, VIDEO_HEIGHT, V4L2_PIX_FMT_YUYV) != 0) {
        goto out_close;
    }

    if(camera_startcap(camera) != 0) {
        goto out_close;
    }

    yuyv_frame = malloc(camera->mapbuffers[0].length);
    if(yuyv_frame == NULL) {
        goto out_stop;
    }

    rgb_frame = malloc(VIDEO_WIDTH * VIDEO_HEIGHT * sizeof(rgb_t));
    if(rgb_frame == NULL) {
        goto out_stop;
    }

    usleep(CAMERA_WARMUP_DELAY_MS * 1000);

    for(int i = 0; i < CAMERA_WARMUP_DROP_FRAMES; i++) {
        if(capute_one_frame(camera, yuyv_frame) != 0) {
            goto out_stop;
        }
    }

    if(capute_one_frame(camera, yuyv_frame) != 0) {
        goto out_stop;
    }

    yuyv422_to_rgb888(yuyv_frame, rgb_frame, VIDEO_WIDTH, VIDEO_HEIGHT);

    if(save_rgb_to_ppm(save_path, rgb_frame, VIDEO_WIDTH, VIDEO_HEIGHT) != 0) {
        goto out_stop;
    }

    ret = 0;

out_stop:
    free(rgb_frame);
    free(yuyv_frame);
    camera_stopcap(camera);

out_close:
    camera_closedev(camera);

out:
    free(camera);
    return ret;
}
