#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/wait.h>
#include <linux/poll.h>

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

#define BTN_NUM 1
#define GPIO_CLEAR_KEY 530
static const int gpio_keys[BTN_NUM] = { GPIO_CLEAR_KEY };
static int irq_keys[BTN_NUM];

static wait_queue_head_t btn_wq;
static int btn_flag = 0;
static int btn_event_idx = -1;

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
    for (int i = 0; i < 4; ++i) {
        iowrite32(0x1 << 5, (void*)(gpio_base + GPCLR0));
        iowrite32(0x1 << 6, (void*)(gpio_base + GPSET0));
        mdelay(100);
        iowrite32(0x1 << 5, (void*)(gpio_base + GPSET0));
        iowrite32(0x1 << 6, (void*)(gpio_base + GPCLR0));
        mdelay(100);
    }
    iowrite32(0x3 << 5, (void*)(gpio_base + GPSET0));
}

static long device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret = 0;

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
        case MY_IOCTL_CMD_BTN_CLEAR:
            printk("dev_Control: MY_IOCTL_CMD_BTN_CLEAR\n");
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
    int value;
    wait_event_interruptible(btn_wq, btn_flag != 0);
    btn_flag = 0;
    value = btn_event_idx;
    btn_event_idx = -1;
    if (copy_to_user(buf, &value, sizeof(int)))
        return -EFAULT;
    return sizeof(int);
}

static unsigned int device_poll(struct file *filp, poll_table *wait)
{
    poll_wait(filp, &btn_wq, wait);
    if (btn_flag)
        return POLLIN | POLLRDNORM;
    return 0;
}

static ssize_t device_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t wlen;

    printk("dev_Control: device_write (minor = %d)\n", iminor(filp->f_path.dentry->d_inode));
    wlen = MAX_BUF;
    if(wlen > count) wlen = count;
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
    .poll = device_poll,
};

// === ISR에서 버튼 이벤트 wakeup ===
static irqreturn_t key_clear_isr(int irq, void *dev_id)
{
    int idx = 0; // 여러 버튼이면 (int)(long)dev_id
    btn_flag = 1;
    btn_event_idx = idx;
    wake_up_interruptible(&btn_wq);
    printk("dev_Control: key_clear_isr - Button pressed!\n");
    return IRQ_HANDLED;
}

// === init/exit ===
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

    // === 버튼 관련 ===
    init_waitqueue_head(&btn_wq);

    for (i = 0; i < BTN_NUM; i++) {
    int ret;
    printk("dev_Control: gpio_request(%d)\n", gpio_keys[i]);
    ret = gpio_request(gpio_keys[i], "btn_gpio");
    if (ret) {
        printk("dev_Control: gpio_request failed for gpio %d, ret = %d\n", gpio_keys[i], ret);
        continue;
    }

    printk("dev_Control: gpio_direction_input(%d)\n", gpio_keys[i]);
    ret = gpio_direction_input(gpio_keys[i]);
    if (ret) {
        printk("dev_Control: gpio_direction_input failed for gpio %d, ret = %d\n", gpio_keys[i], ret);
        gpio_free(gpio_keys[i]);
        continue;
    }

    irq_keys[i] = gpio_to_irq(gpio_keys[i]);
    printk("dev_Control: gpio_to_irq(%d) = %d\n", gpio_keys[i], irq_keys[i]);
    if (irq_keys[i] < 0) {
        printk("dev_Control: gpio_to_irq failed for gpio %d, irq = %d\n", gpio_keys[i], irq_keys[i]);
        gpio_free(gpio_keys[i]);
        continue;
    }

    printk("dev_Control: request_irq(%d)\n", irq_keys[i]);
    ret = request_irq(irq_keys[i], key_clear_isr, IRQF_TRIGGER_FALLING, "key_clear", NULL);
    if (ret) {
        printk("dev_Control: request_irq failed for irq %d, ret = %d\n", irq_keys[i], ret);
        gpio_free(gpio_keys[i]);
        continue;
    }
}
    printk("dev_Control: IRQ enabled for buttons\n");
    return 0;

#if CONF_REQUEST_MEM_REGION_EN
err2:
    release_mem_region(GPIO_PHY_BASE, GPIO_PHY_SIZE);
#endif
err3:
    cdev_del(my_cdev);
err1:
    unregister_chrdev_region(devt, device_minor_count);
err0:
    return ret;
}

static void __exit device_exit(void)
{
    int i;
    printk("dev_Control: device_exit\n");

    for (i = 0; i < BTN_NUM; i++) {
        free_irq(irq_keys[i], NULL);
        gpio_free(gpio_keys[i]);
    }

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