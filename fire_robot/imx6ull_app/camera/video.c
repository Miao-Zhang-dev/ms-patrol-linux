#include "video.h"
#include <stdint.h>

int camera_opendev(camera_t * pcamera)
{
    pcamera->camerafd = open(pcamera->devname, O_RDWR | O_NONBLOCK);
    if(-1 == pcamera->camerafd) {
        return -1;
    }

    return 0;
}

int camera_closedev(camera_t * pcamera)
{
    close(pcamera->camerafd);

    return 0;
}

int camera_showfmt(camera_t * pcamera)
{
    int ret = 0;
    int len = 0;

    //视频设备基本信息
    ret = ioctl(pcamera->camerafd, VIDIOC_QUERYCAP, &pcamera->cameracap);
    if(-1 == ret) {
        return -1;
    }

    int i = 0;
    printf("============= camera info ==============\n");
    printf("Driver: %s\n", pcamera->cameracap.driver);
    printf("Card: %s\n", pcamera->cameracap.card);
    printf("Bus Info: %s\n", pcamera->cameracap.bus_info);
    printf("Version: %d.%d.%d\n", (pcamera->cameracap.version >> 16) & 0xFF, (pcamera->cameracap.version >> 8) & 0xFF,
           pcamera->cameracap.version & 0xFF);
    printf("Capabilities: 0x%08X\n", pcamera->cameracap.capabilities);

    for(pcamera->camerafmt.index = 0;; pcamera->camerafmt.index++) {
        //获取视频设备支持的图像编码格式（YUYV、RGB等）
        pcamera->camerafmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ret                     = ioctl(pcamera->camerafd, VIDIOC_ENUM_FMT, &pcamera->camerafmt);
        if(-1 == ret) {
            if(errno == EINVAL) {
                break;
            }
            return -1;
        }

        printf("Format %d: %s (FourCC: %c%c%c%c)\n", i, pcamera->camerafmt.description,
               (pcamera->camerafmt.pixelformat >> 0) & 0xFF, (pcamera->camerafmt.pixelformat >> 8) & 0xFF,
               (pcamera->camerafmt.pixelformat >> 16) & 0xFF, (pcamera->camerafmt.pixelformat >> 24) & 0xFF);
    }

    memset(&pcamera->frmsize, 0, sizeof(pcamera->frmsize));
    pcamera->frmsize.pixel_format = pcamera->camerafmt.pixelformat;

    for(pcamera->frmsize.index = 0;; pcamera->frmsize.index++) {
        ret = ioctl(pcamera->camerafd, VIDIOC_ENUM_FRAMESIZES, &pcamera->frmsize);
        if(-1 == ret) {
            if(errno == EINVAL) {
                break;
            }
            return -1;
        }

        switch(pcamera->frmsize.type) {
            case V4L2_FRMSIZE_TYPE_DISCRETE:
                printf("Resolution %d: %dx%d\n", pcamera->frmsize.index, pcamera->frmsize.discrete.width,
                       pcamera->frmsize.discrete.height);
                break;
            case V4L2_FRMSIZE_TYPE_STEPWISE:
                printf("Resolution range: %dx%d to %dx%d (step %dx%d)\n", pcamera->frmsize.stepwise.min_width,
                       pcamera->frmsize.stepwise.min_height, pcamera->frmsize.stepwise.max_width,
                       pcamera->frmsize.stepwise.max_height, pcamera->frmsize.stepwise.step_width,
                       pcamera->frmsize.stepwise.step_height);
                break;
        }
    }

    printf("=======================================\n");

    return 0;
}

camera_t * camera_init(const char * pdevname)
{
    camera_t * pcamera = NULL;

    pcamera = malloc(sizeof(camera_t));
    if(pcamera == NULL) {
        return NULL;
    }

    memset(pcamera, 0, sizeof(*pcamera));
    strncpy(pcamera->devname, pdevname, sizeof(pcamera->devname) - 1);

    return pcamera;
}

int camera_setfmt(camera_t * pcamera, int width, int height, unsigned int format)
{
    int ret                   = 0;
    int i                     = 0;
    unsigned long file_length = 0;
    struct v4l2_buffer addbuf;

    memset(&pcamera->setfmt, 0, sizeof(pcamera->setfmt));
    pcamera->setfmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    pcamera->setfmt.fmt.pix.width       = width;
    pcamera->setfmt.fmt.pix.height      = height;
    pcamera->setfmt.fmt.pix.pixelformat = format;
    pcamera->setfmt.fmt.pix.field       = V4L2_FIELD_ANY;
    ret                                 = ioctl(pcamera->camerafd, VIDIOC_S_FMT, &pcamera->setfmt);
    if(-1 == ret) {
        return -1;
    }

    file_length = pcamera->setfmt.fmt.pix.bytesperline * pcamera->setfmt.fmt.pix.height;

    memset(&pcamera->viedoreq, 0, sizeof(pcamera->viedoreq));
    pcamera->viedoreq.count  = 2; //
    pcamera->viedoreq.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    pcamera->viedoreq.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(pcamera->camerafd, VIDIOC_REQBUFS, &pcamera->viedoreq);
    if(-1 == ret) {
        return -1;
    }

    if(pcamera->viedoreq.count < 2) printf("Insufficient buffer memory\n");

    pcamera->mapbuffers = calloc(pcamera->viedoreq.count, sizeof(*pcamera->mapbuffers)); //内存中建立对应空间
    if(NULL == pcamera->mapbuffers) {
        return -1;
    }

    // 5.将申请到的空间映射到用户空间
    for(i = 0; i < pcamera->viedoreq.count; i++) {
        memset(&addbuf, 0, sizeof(addbuf));
        addbuf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        addbuf.memory = V4L2_MEMORY_MMAP;
        addbuf.index  = i;

        ret = ioctl(pcamera->camerafd, VIDIOC_QUERYBUF, &addbuf);
        if(-1 == ret) {
            return -1;
        }

        pcamera->mapbuffers[i].length = addbuf.length;
        pcamera->mapbuffers[i].start =
            mmap(NULL, addbuf.length, PROT_READ | PROT_WRITE, MAP_SHARED, pcamera->camerafd, addbuf.m.offset);
        if(MAP_FAILED == pcamera->mapbuffers[i].start) {
            return -1;
        }

        ret = ioctl(pcamera->camerafd, VIDIOC_QBUF, &addbuf);
        if(-1 == ret) {
            return -1;
        }
    }

    return 0;
}

int get_frame(camera_t * pcamera, unsigned char * pframe)
{
    struct v4l2_buffer tmpbuf;
    unsigned int i = 0;
    int ret        = 0;
    static int cnt = 0;

#if DEBUG
    GET_START_TIME;
#endif

    memset(&tmpbuf, 0, sizeof(tmpbuf));
    tmpbuf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    tmpbuf.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(pcamera->camerafd, VIDIOC_DQBUF, &tmpbuf);
    if(-1 == ret) {
        perror("fail to ioctl");
        return -1;
    }

    memcpy(pframe, pcamera->mapbuffers[tmpbuf.index].start, pcamera->mapbuffers[tmpbuf.index].length);

    ret = ioctl(pcamera->camerafd, VIDIOC_QBUF, &tmpbuf);
    if(-1 == ret) {
        return -1;
    }

#if DEBUG
    GET_END_TIME;
    static long curcnt = 0;
    printf("%ld one frame time:%10.2lfms\n", curcnt, GET_CONST_TIME);
    curcnt++;
#endif

    return 0;
}

int capute_one_frame(camera_t * pcamera, unsigned char * pframe)
{
    fd_set fds;
    struct timeval tv;
    int nready = -1;

    FD_ZERO(&fds);
    FD_SET(pcamera->camerafd, &fds);

    tv.tv_sec  = 2;
    tv.tv_usec = 0;
    for(;;) {
        nready = select(pcamera->camerafd + 1, &fds, NULL, NULL, &tv);
        if(-1 == nready) {
            if(EINTR == errno) continue;
            break;
        }
        if(0 == nready) {
            continue;
        }
        get_frame(pcamera, pframe);
        break;
    }

    return 0;
}

/**
 * @brief 将 YUYV422 数据转换为 RGB888_T 格式（BGR顺序，带保留字节）
 * @param yuyv_input  输入 YUYV422 数据指针（每4字节表示2像素：Y0 U Y1 V）
 * @param rgb_output  输出 RGB888_T 数据指针（每像素4字节：B G R Reserved）
 * @param width       图像宽度（必须为偶数）
 * @param height      图像高度
 * @note 转换公式基于 BT.601 标准[3](@ref)[6](@ref)，优化整数运算避免浮点开销[4](@ref)
 */
void yuyv422_to_rgb888(const unsigned char * yuyv_input, rgb_t * rgb_output, int width, int height)
{
    int y = 0;
    int x = 0;

    for(y = 0; y < height; y++) {
        for(x = 0; x < width; x += 2) {
            // 提取 YUYV 分量（大端序）
            unsigned char y0 = yuyv_input[0]; // 像素1 Y
            unsigned char u  = yuyv_input[1]; // 共享 U
            unsigned char y1 = yuyv_input[2]; // 像素2 Y
            unsigned char v  = yuyv_input[3]; // 共享 V
            yuyv_input += 4;

            // 调整 YUV 范围（Y:16-235→0-255, UV:16-240→-128~127）[5](@ref)[6](@ref)
            int y0_adj = y0 - 16;
            int y1_adj = y1 - 16;
            int u_adj  = u - 128;
            int v_adj  = v - 128;

            // 计算 RGB 值（整数运算优化，系数缩放1024倍）[3](@ref)[4](@ref)
            int r0 = y0_adj + ((v_adj * 1436) >> 10);              // 1.402 ≈ 1436/1024
            int g0 = y0_adj - ((u_adj * 352 + v_adj * 731) >> 10); // 0.344≈352/1024, 0.714≈731/1024
            int b0 = y0_adj + ((u_adj * 1814) >> 10);              // 1.772 ≈ 1814/1024

            int r1 = y1_adj + ((v_adj * 1436) >> 10);
            int g1 = y1_adj - ((u_adj * 352 + v_adj * 731) >> 10);
            int b1 = y1_adj + ((u_adj * 1814) >> 10);

// 钳位到 0-255 范围[4](@ref)
#define CLAMP(x) ((x) < 0 ? 0 : ((x) > 255 ? 255 : (x)))
            rgb_output[x].B        = (unsigned char)CLAMP(b0);
            rgb_output[x].G        = (unsigned char)CLAMP(g0);
            rgb_output[x].R        = (unsigned char)CLAMP(r0);
            rgb_output[x].Reserved = 0; // 保留字节置零

            rgb_output[x + 1].B        = (unsigned char)CLAMP(b1);
            rgb_output[x + 1].G        = (unsigned char)CLAMP(g1);
            rgb_output[x + 1].R        = (unsigned char)CLAMP(r1);
            rgb_output[x + 1].Reserved = 0;
        }
        rgb_output += width; // 移动到下一行
    }
}

int camera_stopcap(camera_t * pcamera)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int ret                 = 0;

    ret = ioctl(pcamera->camerafd, VIDIOC_STREAMOFF, &type);
    if(-1 == ret) {
        return -1;
    }

    return 0;
}

int camera_startcap(camera_t * pcamera)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int ret                 = 0;

    ret = ioctl(pcamera->camerafd, VIDIOC_STREAMON, &type);
    if(-1 == ret) {
        return -1;
    }

    return 0;
}

void camera_deinit(camera_t * pcamera)
{
    unsigned int i;

    if(pcamera == NULL) {
        return;
    }

    if(pcamera->mapbuffers != NULL) {
        for(i = 0; i < pcamera->viedoreq.count; i++) {
            if(pcamera->mapbuffers[i].start != NULL && pcamera->mapbuffers[i].start != MAP_FAILED) {
                munmap(pcamera->mapbuffers[i].start, pcamera->mapbuffers[i].length);
            }
        }

        free(pcamera->mapbuffers);
        pcamera->mapbuffers = NULL;
    }

    free(pcamera);
}
