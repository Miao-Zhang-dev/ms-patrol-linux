#include "sensor_manager.h"
#include "mq2_sensor.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int g_sht30_fd  = -1;
static int g_bh1750_fd = -1;
static int g_flame_fd  = -1;
static mq2_dev_t g_mq2_dev;
static int g_mq2_ready = 0;

static int read_exact(int fd, void * buf, size_t len)
{
    unsigned char * p = (unsigned char *)buf;
    size_t done       = 0;

    while(done < len) {
        ssize_t ret = read(fd, p + done, len - done);
        if(ret < 0) {
            if(errno == EINTR) {
                continue;
            }
            return -errno;
        }

        if(ret == 0) {
            return -EIO;
        }

        done += (size_t)ret;
    }

    return 0;
}

static int read_sht30(sensor_data_t * data)
{
    unsigned char buf[6] = {0};
    unsigned short raw_temp;
    unsigned short raw_humi;
    int ret;

    ret = read_exact(g_sht30_fd, buf, sizeof(buf));
    if(ret < 0) {
        return ret;
    }

    raw_temp = ((unsigned short)buf[0] << 8) | buf[1];
    raw_humi = ((unsigned short)buf[3] << 8) | buf[4];

    data->temperature = -45.0f + 175.0f * (float)raw_temp / 65535.0f;
    data->humidity    = 100.0f * (float)raw_humi / 65535.0f;
    data->valid_mask |= SENSOR_VALID_SHT30;

    return 0;
}

static int read_bh1750(sensor_data_t * data)
{
    unsigned char buf[2] = {0};
    unsigned short raw_lux;
    int ret;

    ret = read_exact(g_bh1750_fd, buf, sizeof(buf));
    if(ret < 0) {
        return ret;
    }

    raw_lux         = ((unsigned short)buf[0] << 8) | buf[1];
    data->light_lux = (float)raw_lux / 1.2f;
    data->valid_mask |= SENSOR_VALID_BH1750;

    return 0;
}

static int read_mq2(sensor_data_t * data)
{
    float raw_volt;
    float real_ao;

    if(!g_mq2_ready) {
        return -ENODEV;
    }

    raw_volt = mq2_read_raw_volt(&g_mq2_dev);
    if(raw_volt < 0.0f) {
        return -EIO;
    }

    real_ao = mq2_get_real_ao(&g_mq2_dev);
    if(real_ao < 0.0f) {
        return -EIO;
    }

    data->mq2_raw_volt     = raw_volt;
    data->mq2_real_ao_volt = real_ao;
    data->valid_mask |= SENSOR_VALID_MQ2;

    return 0;
}

static int read_flame(sensor_data_t * data)
{
    int status = 0;
    int ret;

    ret = read_exact(g_flame_fd, &status, sizeof(status));
    if(ret < 0) {
        return ret;
    }

    data->flame_detected = status ? 0 : 1;
    data->valid_mask |= SENSOR_VALID_FLAME;

    return 0;
}

int sensor_manager_init(void)
{
    g_sht30_fd = open(SHT30_DEV_PATH, O_RDONLY);
    if(g_sht30_fd < 0) {
        perror("open " SHT30_DEV_PATH);
        goto fail;
    }

    g_bh1750_fd = open(BH1750_DEV_PATH, O_RDONLY);
    if(g_bh1750_fd < 0) {
        perror("open " BH1750_DEV_PATH);
        goto fail;
    }

    g_flame_fd = open(FLAME_DEV_PATH, O_RDONLY);
    if(g_flame_fd < 0) {
        perror("open " FLAME_DEV_PATH);
        goto fail;
    }

    if(mq2_init(&g_mq2_dev) != 0) {
        fprintf(stderr, "mq2_init failed\n");
        goto fail;
    }

    g_mq2_ready = 1;
    return 0;

fail:
    sensor_manager_deinit();
    return -1;
}

int sensor_manager_read_all(sensor_data_t * data)
{
    int error_count = 0;

    if(data == NULL) {
        return -EINVAL;
    }

    memset(data, 0, sizeof(*data));
    data->timestamp = time(NULL);

    if(read_sht30(data) < 0) {
        error_count++;
    }

    if(read_bh1750(data) < 0) {
        error_count++;
    }

    if(read_mq2(data) < 0) {
        error_count++;
    }

    if(read_flame(data) < 0) {
        error_count++;
    }

    return error_count == 0 ? 0 : -error_count;
}

void sensor_manager_deinit(void)
{
    if(g_sht30_fd >= 0) {
        close(g_sht30_fd);
        g_sht30_fd = -1;
    }

    if(g_bh1750_fd >= 0) {
        close(g_bh1750_fd);
        g_bh1750_fd = -1;
    }

    if(g_flame_fd >= 0) {
        close(g_flame_fd);
        g_flame_fd = -1;
    }

    g_mq2_ready = 0;
}