#ifndef VIDEO_H
#define VIDEO_H

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define VIDEO_WIDTH 640
#define VIDEO_HEIGHT 480

typedef struct
{
    unsigned char B;
    unsigned char G;
    unsigned char R;
    unsigned char Reserved;
} rgb_t;

typedef struct mapbuffer
{
    void * start;
    size_t length;
} mapbuffer_t;

typedef struct uvc_camera
{
    char devname[20];

    int camerafd;

    struct v4l2_fmtdesc camerafmt;
    struct v4l2_capability cameracap;
    struct v4l2_frmsizeenum frmsize;
    struct v4l2_format setfmt;
    struct v4l2_requestbuffers viedoreq;

    mapbuffer_t * mapbuffers;
    unsigned int mapbuffers_cnt;
} camera_t;

int camera_opendev(camera_t * pcamera);
int camera_closedev(camera_t * pcamera);
int camera_showfmt(camera_t * pcamera);
camera_t * camera_init(const char * pdevname);
int camera_setfmt(camera_t * pcamera, int width, int height, unsigned int format);
int capute_one_frame(camera_t * pcamera, unsigned char * pframe);
int get_frame(camera_t * pcamera, unsigned char * pframe);
void yuyv422_to_rgb888(const unsigned char * yuyv_input, rgb_t * rgb_output, int width, int height);
int camera_startcap(camera_t * pcamera);
int camera_stopcap(camera_t * pcamera);
void camera_deinit(camera_t *pcamera);

#endif