#ifndef LED_LED_H
#define LED_LED_H

#include <stdio.h>

struct led_device
{
    FILE *filp;
};

int led_init_device(struct led_device *dev);
int led_turn_on(struct led_device *dev, int led);
int led_turn_off(struct led_device *dev, int led);
int led_release_device(struct led_device *dev);

#endif // LED_LED_H