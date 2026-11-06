#include "asm-generic/errno-base.h"
#include "asm-generic/gpio.h"
#include "asm/gpio.h"
#include "linux/err.h"
#include "linux/export.h"
#include "linux/of_address.h"
#include "linux/of_gpio.h"
#include "linux/printk.h"
#include "linux/string.h"
#include "linux/sysfs.h"
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/of.h>

#define DEV_NAME "flame_sensor"

struct device_node *pnode = NULL;

static ssize_t myread(struct file *f, char __user *userdata, size_t size, loff_t *off);
static ssize_t mywrite(struct file *f, const char __user *userdata, size_t size, loff_t *off);
static int myopen(struct inode *inode, struct file *f);
static int myrelease(struct inode *inode, struct file *f);
static ssize_t myshow(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t mystore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);

static struct device_attribute attr = __ATTR(flame_status, 0664, myshow, mystore);

static int flame_gpio = 0;

struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = myopen,
    .release = myrelease,
    .read = myread,
    .write = mywrite,
};

static struct miscdevice misc_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEV_NAME,
    .fops = &fops,
};

typedef enum
{
  NO_FLAME = 0,
  FLAME_DETECTED = 1
} FLAME_STATE;

static FLAME_STATE status;

/*
 * 读取sysfs属性时调用，返回当前火焰状态字符串
 */
static ssize_t myshow(struct device *dev, struct device_attribute *attr, char *buf)
{
  ssize_t len = 0;
  /* 每次读取前实时获取GPIO电平，更新状态 */
  status = gpio_get_value(flame_gpio) ? NO_FLAME : FLAME_DETECTED;

  if (FLAME_DETECTED == status)
  {
    strcpy(buf, "FLAME_DETECTED\n");
  }
  else
  {
    strcpy(buf, "NO_FLAME\n");
  }
  len = strlen(buf);
  return len;
}

/*
 * 写入sysfs属性时调用
 * 火焰传感器为输入设备，写入无实际控制效果，仅保留接口兼容
 */
static ssize_t mystore(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
  pr_info("flame sensor is input device, write has no effect\n");
  return count;
}

/*
 * read系统调用，向用户空间返回火焰状态数值
 * 0 = 无火焰，1 = 检测到火焰
 */
ssize_t myread(struct file *f, char __user *userdata, size_t size, loff_t *off)
{
  unsigned long ret = 0;

  /* 实时读取GPIO电平更新状态：低电平=有火焰，高电平=无火焰 */
  status = gpio_get_value(flame_gpio) ? NO_FLAME : FLAME_DETECTED;

  if (size < sizeof(status))
  {
    pr_err("read buffer too small\n");
    return -EINVAL;
  }

  ret = copy_to_user(userdata, &status, sizeof(status));
  if (ret)
  {
    pr_err("copy_to_user error\n");
    return -EFAULT;
  }
  return sizeof(status);
}

/*
 * write系统调用
 * 输入设备不支持写入控制，仅保留接口，写入不生效
 */
ssize_t mywrite(struct file *f, const char __user *userdata, size_t size, loff_t *off)
{
  pr_info("flame sensor is input device, write has no effect\n");
  return size;
}

int myopen(struct inode *inode, struct file *f)
{
  pr_info("flame sensor open\n");
  return 0;
}

int myrelease(struct inode *inode, struct file *f)
{
  pr_info("flame sensor close\n");
  return 0;
}

static int __init flame_drv_init(void)
{
  int ret = 0;

  /* 1. 注册杂项设备 */
  ret = misc_register(&misc_dev);
  if (ret)
  {
    pr_err("misc_register error, errno %d\n", ret);
    return ret;
  }

  /* 2. 查找设备树节点 */
  pnode = of_find_node_by_path("/flame_sensor");
  if (!pnode)
  {
    ret = -ENODEV;
    pr_err("of_find_node_by_path error, errno:%d\n", ret);
    goto del_misc;
  }

  /* 3. 从设备树获取GPIO编号，对应dts里的gpios属性 */
  flame_gpio = of_get_named_gpio(pnode, "gpios", 0);
  if (flame_gpio < 0)
  {
    ret = flame_gpio;
    pr_err("of_get_named_gpio error, errno:%d\n", ret);
    goto del_misc;
  }

  /* 4. 申请GPIO */
  ret = gpio_request(flame_gpio, "flame_gpio");
  if (ret)
  {
    pr_err("gpio_request error, errno:%d\n", ret);
    goto del_misc;
  }

  /* 5. 设置GPIO为输入模式（和LED驱动的核心区别） */
  ret = gpio_direction_input(flame_gpio);
  if (ret)
  {
    pr_err("gpio_direction_input error, errno:%d\n", ret);
    goto del_gpio;
  }

  /* 初始化读取一次状态 */
  status = gpio_get_value(flame_gpio) ? NO_FLAME : FLAME_DETECTED;

  pr_info("major:10 , minor:%d\n", misc_dev.minor);

  /* 6. 创建sysfs属性文件 */
  ret = device_create_file(misc_dev.this_device, &attr);
  if (ret)
  {
    pr_err("device_create_file error\n");
    goto del_gpio;
  }

  pr_info("flame sensor driver init ok\n");
  return 0;

del_gpio:
  gpio_free(flame_gpio);
del_misc:
  misc_deregister(&misc_dev);
  return ret;
}

static void __exit flame_drv_exit(void)
{
  device_remove_file(misc_dev.this_device, &attr);
  gpio_free(flame_gpio);
  misc_deregister(&misc_dev);
  pr_info("flame sensor driver exit\n");
  return;
}

module_init(flame_drv_init);
module_exit(flame_drv_exit);
MODULE_LICENSE("GPL");
