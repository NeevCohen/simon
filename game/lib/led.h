#ifndef LED_LED_H
#define LED_LED_H

#include <stdio.h>

#define NUM_LEDS 4
#define NUM_BTNS 4

#define OFF 0
#define ON 1

struct led_device
{
    FILE *filp;
};

int led_init_device(struct led_device *dev);
int control_led(struct led_device *dev, int led, int command);
int led_turn_on(struct led_device *dev, int led);
int led_turn_off(struct led_device *dev, int led);
int led_turn_on_all(struct led_device *dev);
int led_turn_off_all(struct led_device *dev);
int led_release_device(struct led_device *dev);

#endif // LED_LED_H