#include "camera_capture.h"

#include <stdio.h>

int main(void)
{
    int ret;

    ret = camera_capture_once("/tmp/alarm_test.ppm");
    if(ret != 0) {
        printf("camera_capture_once failed\n");
        return -1;
    }

    printf("camera_capture_once ok: /tmp/alarm_test.ppm\n");
    return 0;
}