#include "gpio_control.h"
#include "../include/custom_ioctl.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstdio>
#include <thread>
#include <chrono>

// 디바이스 파일 경로 (필요시 환경에 맞게 수정)
#define GPIO_DEV_PATH "/dev/mydev"

void gpio_led_correct() {
    int fd = open(GPIO_DEV_PATH, O_RDWR);
    if (fd < 0) {
        perror("open /dev/mydev");
        return;
    }
    // LED ON
    ioctl(fd, MY_IOCTL_CMD_LED_ON);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    // LED OFF
    ioctl(fd, MY_IOCTL_CMD_LED_OFF);
    close(fd);
}

void gpio_led_wrong() {
    int fd = open(GPIO_DEV_PATH, O_RDWR);
    if (fd < 0) {
        perror("open /dev/my_device");
        return;
    }
    ioctl(fd, MY_IOCTL_CMD_LED_BLINK);
    close(fd);
}