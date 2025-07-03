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
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            // LED OFF
            ioctl(fd, MY_IOCTL_CMD_LED_OFF);
            break;
        case LED_WRONG:
            ioctl(fd, MY_IOCTL_CMD_LED_BLINK);
            break;
        case BTN_CLEAR:
            ioctl(fd, MY_IOCTL_CMD_BTN_CLEAR);
            break;
        default:
            break; 
    }
    close(fd);
}