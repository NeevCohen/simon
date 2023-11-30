#ifndef SIMON_SIMON_H
#define SIMON_SIMON_H

#include <stdio.h>

struct led_device {
	FILE *filp;
};

int simon_init_led_device(struct led_device *dev);
int simon_turn_led_on(struct led_device *dev, int led);
int simon_turn_led_off(struct led_device *dev, int led);

#endif // SIMON_SIMON_H
