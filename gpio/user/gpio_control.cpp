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

void handle_device_control_request(requestType requestType)
{
    int fd = open(GPIO_DEV_PATH, O_RDWR);
    if (fd < 0) {
        perror("open /dev/mydev");
        return;
    }

    switch(requestType){
        case LED_CORRECT:
            ioctl(fd, MY_IOCTL_CMD_LED_ON);
            break;
        case LED_WRONG:
            ioctl(fd, MY_IOCTL_CMD_LED_BLINK);
            break;
        case BTN_CLEAR:
            ioctl(fd, MY_IOCTL_CMD_BTN_CLEAR);
            break;
        case LED_TIMER:
            ioctl(fd, MY_IOCTL_CMD_LED_TIMER);
            break;
        default:
            break; 
    }
    close(fd);
}