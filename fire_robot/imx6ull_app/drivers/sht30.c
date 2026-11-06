#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/mutex.h>

#define DEV_NAME "robotsht30"

#define SHT30_CMD_MEAS_HIGH_NO_HOLD_MSB 0x24
#define SHT30_CMD_MEAS_HIGH_NO_HOLD_LSB 0x00
#define SHT30_CMD_SOFT_RESET_MSB 0x30
#define SHT30_CMD_SOFT_RESET_LSB 0xA2

static struct i2c_client * g_client;
static DEFINE_MUTEX(sht30_mutex);

static int sht30_open(struct inode * inode, struct file * file)
{
    pr_info("sht30_open called\n");
    return 0;
}

static int sht30_write_cmd(struct i2c_client * client, u8 msb, u8 lsb)
{
    u8 cmd[2] = {msb, lsb};
    int ret;
    int i;

    for(i = 0; i < 3; i++) {
        ret = i2c_master_send(client, cmd, sizeof(cmd));
        if(ret == sizeof(cmd)) {
            return 0;
        }

        pr_err("sht30 send cmd %02x %02x fail ret:%d\n", msb, lsb, ret);
        msleep(20);
    }

    return ret < 0 ? ret : -EIO;
}

static ssize_t sht30_read(struct file * file, char __user * userdata, size_t size, loff_t * off)
{
    u8 buf[6] = {0};
    int ret;
    int i;

    if(!g_client) {
        return -ENODEV;
    }

    if(size < sizeof(buf)) {
        return -EINVAL;
    }

    mutex_lock(&sht30_mutex);

    ret = sht30_write_cmd(g_client, SHT30_CMD_MEAS_HIGH_NO_HOLD_MSB, SHT30_CMD_MEAS_HIGH_NO_HOLD_LSB);
    if(ret < 0) {
        goto out_unlock;
    }

    msleep(20);

    for(i = 0; i < 3; i++) {
        ret = i2c_master_recv(g_client, buf, sizeof(buf));
        if(ret == sizeof(buf)) {
            ret = 0;
            break;
        }

        pr_err("sht30 recv fail ret:%d\n", ret);
        msleep(20);
    }

    if(ret != 0) {
        ret = -EIO;
        goto out_unlock;
    }

    if(copy_to_user(userdata, buf, sizeof(buf))) {
        ret = -EFAULT;
        goto out_unlock;
    }

    ret = sizeof(buf);

out_unlock:
    mutex_unlock(&sht30_mutex);
    return ret;
}

static int sht30_release(struct inode * node, struct file * file)
{
    pr_info("sht30_release called\n");
    return 0;
}

static const struct file_operations sht30_fops = {
    .owner   = THIS_MODULE,
    .open    = sht30_open,
    .read    = sht30_read,
    .release = sht30_release,
};

static struct miscdevice sht30_miscdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = DEV_NAME,
    .fops  = &sht30_fops,
};

static int sht30_probe(struct i2c_client * client, const struct i2c_device_id * id)
{
    int ret;

    g_client = client;

    ret = sht30_write_cmd(client, SHT30_CMD_SOFT_RESET_MSB, SHT30_CMD_SOFT_RESET_LSB);
    if(ret < 0) {
        pr_err("sht30 soft reset failed:%d\n", ret);
    }

    msleep(20);

    ret = misc_register(&sht30_miscdev);
    if(ret < 0) {
        pr_err("sht30 misc_register err:%d\n", ret);
        g_client = NULL;
        return ret;
    }

    pr_info("sht30 probe ok, i2c addr:0x%02x\n", client->addr);
    return 0;
}

static int sht30_remove(struct i2c_client * client)
{
    misc_deregister(&sht30_miscdev);
    g_client = NULL;
    return 0;
}

static const struct of_device_id sht30_of_match[] = {{.compatible = "robot,sht30"}, {}};
MODULE_DEVICE_TABLE(of, sht30_of_match);

static const struct i2c_device_id sht30_id[] = {{"robotsht30", 0}, {}};
MODULE_DEVICE_TABLE(i2c, sht30_id);

static struct i2c_driver sht30_driver = {
    .probe  = sht30_probe,
    .remove = sht30_remove,
    .driver =
        {
            .name           = "robotsht30",
            .owner          = THIS_MODULE,
            .of_match_table = sht30_of_match,
        },
    .id_table = sht30_id,
};

module_i2c_driver(sht30_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Loophole");
MODULE_DESCRIPTION("SHT30 misc i2c driver");