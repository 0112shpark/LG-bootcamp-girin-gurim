#ifndef GPIO_CONTROL_H
#define GPIO_CONTROL_H

#include "../include/custom_ioctl.h"

enum requestType {
    LED_CORRECT,
    LED_WRONG,
    BTN_CLEAR
};

void handle_device_control_request(requestType requestType);

#endif // GPIO_CONTROL_H