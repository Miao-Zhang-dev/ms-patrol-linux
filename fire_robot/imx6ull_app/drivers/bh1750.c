#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/mutex.h>

#define DEV_NAME "robotbh1750"

#define BH1750_CMD_POWER_ON 0x01
#define BH1750_CMD_RESET 0x07
#define BH1750_CMD_ONE_TIME_H_RES 0x20

static struct i2c_client * g_client;
static DEFINE_MUTEX(bh1750_mutex);

static int bh1750_open(struct inode * inode, struct file * file)
{
    pr_info("bh1750_open called\n");
    return 0;
}

static int bh1750_write_cmd(struct i2c_client * client, u8 cmd)
{
    int ret;
    int i;

    for(i = 0; i < 3; i++) {
        ret = i2c_master_send(client, &cmd, 1);
        if(ret == 1) {
            return 0;
        }

        pr_err("bh1750 send cmd 0x%02x fail ret:%d\n", cmd, ret);
        msleep(20);
    }

    return ret < 0 ? ret : -EIO;
}

static ssize_t bh1750_read(struct file * file, char __user * userdata, size_t size, loff_t * off)
{
    u8 buf[2] = {0};
    int ret;
    int i;

    if(!g_client) {
        return -ENODEV;
    }

    if(size < sizeof(buf)) {
        return -EINVAL;
    }

    mutex_lock(&bh1750_mutex);

    ret = bh1750_write_cmd(g_client, BH1750_CMD_POWER_ON);
    if(ret < 0) {
        goto out_unlock;
    }

    ret = bh1750_write_cmd(g_client, BH1750_CMD_ONE_TIME_H_RES);
    if(ret < 0) {
        goto out_unlock;
    }

    msleep(180);

    for(i = 0; i < 3; i++) {
        ret = i2c_master_recv(g_client, buf, sizeof(buf));
        if(ret == sizeof(buf)) {
            ret = 0;
            break;
        }

        pr_err("bh1750 recv fail ret:%d\n", ret);
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
    mutex_unlock(&bh1750_mutex);
    return ret;
}

static int bh1750_release(struct inode * node, struct file * file)
{
    pr_info("bh1750_release called\n");
    return 0;
}

static const struct file_operations bh1750_fops = {
    .owner   = THIS_MODULE,
    .open    = bh1750_open,
    .read    = bh1750_read,
    .release = bh1750_release,
};

static struct miscdevice bh1750_miscdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = DEV_NAME,
    .fops  = &bh1750_fops,
};

static int bh1750_probe(struct i2c_client * client, const struct i2c_device_id * id)
{
    int ret;

    g_client = client;

    ret = bh1750_write_cmd(client, BH1750_CMD_POWER_ON);
    if(ret < 0) {
        pr_err("bh1750 power on failed:%d\n", ret);
        g_client = NULL;
        return ret;
    }

    msleep(10);

    bh1750_write_cmd(client, BH1750_CMD_RESET);
    msleep(10);

    ret = misc_register(&bh1750_miscdev);
    if(ret < 0) {
        pr_err("bh1750 misc_register error:%d\n", ret);
        g_client = NULL;
        return ret;
    }

    pr_info("bh1750_probe ok, i2c addr:0x%02x\n", client->addr);
    return 0;
}

static int bh1750_remove(struct i2c_client * client)
{
    misc_deregister(&bh1750_miscdev);
    g_client = NULL;
    return 0;
}

static const struct of_device_id bh1750_of_match[] = {{.compatible = "robot,bh1750"}, {}};
MODULE_DEVICE_TABLE(of, bh1750_of_match);

static const struct i2c_device_id bh1750_id[] = {{"robotbh1750", 0}, {}};
MODULE_DEVICE_TABLE(i2c, bh1750_id);

static struct i2c_driver bh1750_driver = {
    .probe  = bh1750_probe,
    .remove = bh1750_remove,
    .driver =
        {
            .name           = "robotbh1750",
            .owner          = THIS_MODULE,
            .of_match_table = bh1750_of_match,
        },
    .id_table = bh1750_id,
};

module_i2c_driver(bh1750_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Loophole");
MODULE_DESCRIPTION("BH1750 misc i2c driver");