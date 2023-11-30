#include <stdio.h>
#include <string.h>

#include "simon.h"

#define NUM_LEDS 4
#define NUM_BTNS 4

#define OFF 0
#define ON 1

#define LED_DEV_FILE_PATH "/proc/led"

int simon_init_led_device(struct led_device *dev) {
	dev->filp = fopen(LED_DEV_FILE_PATH, "r+");
	if (!dev->filp) {
		perror("fopen");
		return -1;
	}

	return 0;
};

static int control_led(struct led_device *dev, int led, int command) {
	size_t written = 0;
	char command_str[4];

	if (led < 0 || led > NUM_LEDS - 1) {
		printf("[control_led] LED number is invalid\n");
		return -1;
	}

	if (snprintf(command_str, sizeof(command_str), "%d,%d", led, command) != sizeof(command_str) - 1) {
		printf("[control_led] Failed to create command string\n");
		return -1;
	}

	written = fwrite(command_str, strlen(command_str), 1, dev->filp);
	if (!written) {
		printf("[control_led] Failed to write command to file\n");
		return -1;
	}

	if (fflush(dev->filp) < 0){
		printf("[control_led] Failed to flush LED file\n");
	}

	return 0;
};

int simon_turn_led_on(struct led_device *dev, int led) {
	return control_led(dev, led, ON);
};

int simon_turn_led_off(struct led_device *dev, int led) {
	return control_led(dev, led, OFF);
};