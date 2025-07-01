#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <asm/io.h>

#include "../include/custom_ioctl.h"

#define MAX_BUF 26

static unsigned int device_major = 120;
static unsigned int device_minor_start = 0;
static unsigned int device_minor_count = 4;
static dev_t devt;
static struct cdev *my_cdev;

static char rbuf[MAX_BUF];
static char wbuf[MAX_BUF];

#define GPIO_PHY_BASE           0x3f200000
#define GPIO_PHY_SIZE           0x100
#define GPFSEL0                 0x000
#define GPSET0                  0x01c
#define GPCLR0                  0x028

#define CONF_REQUEST_MEM_REGION_EN 0

static volatile unsigned long gpio_base;
#if CONF_REQUEST_MEM_REGION_EN
static struct resource *gpio_mem;
#endif

static void led_init(void)
{
        iowrite32((ioread32((void*)(gpio_base+GPFSEL0)) & ~(0x3f<<15)) | (0x9<<15), (void*)(gpio_base+GPFSEL0));
        iowrite32(0x3<<5, (void*)(gpio_base+GPSET0));
}

static void led_on(void)
{
        iowrite32(0x3<<5, (void*)(gpio_base+GPCLR0));
}

static void led_off(void)
{
        iowrite32(0x3<<5, (void*)(gpio_base+GPSET0));
}

static void led_blink(void)
{
    for (int i = 0; i < 7; ++i) {

        iowrite32(0x1 << 5, (void*)(gpio_base + GPCLR0));
        iowrite32(0x1 << 6, (void*)(gpio_base + GPSET0));
        mdelay(300);
        iowrite32(0x1 << 5, (void*)(gpio_base + GPSET0));
        iowrite32(0x1 << 6, (void*)(gpio_base + GPCLR0));
        mdelay(300);
    }
    iowrite32(0x3 << 5, (void*)(gpio_base + GPSET0));
}

static long device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
        int ret = 0, data;

        printk("dev_Control : device_ioctl (minor = %d)\n", iminor(filp->f_path.dentry->d_inode));
        switch(cmd) {
                case MY_IOCTL_CMD_LED_ON:
                        printk("dev_Control: MY_IOCTL_CMD_LED_ON\n");
                        led_on();
                        break;
                case MY_IOCTL_CMD_LED_OFF:
                        printk("dev_Control: MY_IOCTL_CMD_LED_OFF\n");
                        led_off();
                        break;
                case MY_IOCTL_CMD_LED_BLINK:
                        printk("dev_Control: MY_IOCTL_CMD_LED_BLINK\n");
                        led_blink();
                        break;
                default:
                        printk("dev_Control: unknown command\n");
                        ret = -EINVAL;
                        break;
        }

        return ret;
}

static ssize_t device_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
        ssize_t rlen;

        printk("devtest: device_read (minor = %d)\n", iminor(filp->f_path.dentry->d_inode));
        rlen = MAX_BUF;
        if(rlen > count) {
                rlen = count;
        }
        if(copy_to_user(buf, rbuf, rlen)) {
                return -EFAULT;
        }
        printk("devtest: read %ld bytes\n", rlen);

        return rlen;
}

static ssize_t device_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
        ssize_t wlen;

        printk("dev_Control: device_write (minor = %d)\n", iminor(filp->f_path.dentry->d_inode));
        wlen = MAX_BUF;
        if(wlen > count) {
                wlen = count;
        }
        if(copy_from_user(wbuf, buf, wlen)) {
                return -EFAULT;
        }
        printk("dev_Control: wrote %ld bytes\n", wlen);

        return wlen;
}

static int device_open(struct inode *inode, struct file *filp)
{
        printk("dev_Control: device_open (minor = %d)\n", iminor(inode));
        return 0;
}

static int device_release(struct inode *inode, struct file *filp)
{
        printk("dev_Control: device_release\n");
        return 0;
}


static const struct file_operations my_fops = {
        .owner = THIS_MODULE,
        .open = device_open,
        .release = device_release,
        .read = device_read,
        .write = device_write,
        .unlocked_ioctl = device_ioctl,
};

static int __init device_init(void)
{
        int ret, i;

        printk("dev_Control: device_init\n");

        devt = MKDEV(device_major, device_minor_start);
        ret = register_chrdev_region(devt, device_minor_count, "my_device");
        if(ret < 0) {
                printk("dev_Control: can't get major %d\n", device_major);
                goto err0;
        }

        my_cdev = cdev_alloc();
        my_cdev->ops = &my_fops;
        my_cdev->owner = THIS_MODULE;
        ret = cdev_add(my_cdev, devt, device_minor_count);
        if(ret) {
                printk("dev_Control: can't add device %d\n", devt);
                goto err1;
        }

        /* init buffers */
        for(i=0; i<MAX_BUF; i++) rbuf[i] = 'A' + i;
        for(i=0; i<MAX_BUF; i++) wbuf[i] = 0;

#if CONF_REQUEST_MEM_REGION_EN
        gpio_mem = request_mem_region(GPIO_PHY_BASE, GPIO_PHY_SIZE, "gpio");
        if (gpio_mem == NULL) {
                printk("dev_Control: failed to get memory region\n");
                ret = -EIO;
                goto err2;
        }
#endif

        gpio_base = (unsigned long)ioremap(GPIO_PHY_BASE, GPIO_PHY_SIZE);
        if (gpio_base == 0) {
                printk("dev_Control: ioremap error\n");
                ret = -EIO;
                goto err3;
        }

        led_init();

        return 0;

err3:
#if CONF_REQUEST_MEM_REGION_EN
        release_mem_region(GPIO_PHY_BASE, GPIO_PHY_SIZE);
err2:
#endif
        cdev_del(my_cdev);
err1:
        unregister_chrdev_region(devt, device_minor_count);
err0:
        return ret;
}

static void __exit device_exit(void)
{
        printk("devtest: device_exit\n");

        iounmap((void *)gpio_base);
#if CONF_REQUEST_MEM_REGION_EN
        release_mem_region(GPIO_PHY_BASE, GPIO_PHY_SIZE);
#endif
        cdev_del(my_cdev);
        unregister_chrdev_region(devt, device_minor_count);
}

module_init(device_init);
module_exit(device_exit);

MODULE_LICENSE("GPL");