#include <stdio.h>
#include <unistd.h>

#include "simon.h"

int main(int argc, char **argv) {
	struct led_device dev;
	int ret = 0, i;

	if ((ret = simon_init_led_device(&dev) < 0)) {
		printf("[simon] Failed to initialize led device\n");
		return ret;
	}

	for (i = 0; i < 4; ++i) {
		if ((ret = simon_turn_led_on(&dev, i))) {
			printf("[simon] Failed to turn on LED %d\n", i);
			return ret;
		}
		sleep(1);
	}

	sleep(1);

	for (i = 3; i > -1; --i) {
		if ((ret = simon_turn_led_off(&dev, i))) {
			printf("[simon] Failed to turn off LED %d\n", i);
			return ret;
		}
		sleep(1);
	}


	return ret;
}

