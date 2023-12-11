#include <sys/time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>

#include "led.h"

#define USEC_IN_MSEC 1000

static inline long long msec_to_usec(long long msec) {
	return msec * USEC_IN_MSEC;
}

static inline long long usec_to_msec(long long usec) {
	return usec / USEC_IN_MSEC;
}

long long time_in_ms(void) {
    struct timeval tv;

    gettimeofday(&tv,NULL);
    return (((long long)tv.tv_sec)*1000)+(tv.tv_usec/1000);
}

int generate_random_number(int min, int max) {
	return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}

/*
Run in loop, turning all leds on and off until a user enters the start command
*/
int turn_leds_on_off_until_input_loop(struct led_device *dev) {
	int led = 0, command = ON;
	struct pollfd poll_fd;
	size_t s;
	char buf[2];
	long long last_led_toggle_time_ms = time_in_ms();


	poll_fd.fd = fileno(dev->filp);
	poll_fd.events = POLLIN | POLLOUT;

	while (1) {
		if (poll(&poll_fd, 1, -1) < 0) {
			perror("poll");
			return EXIT_FAILURE;
		}

		for (nfds_t i = 0; i < 1; ++i) {
			if (poll_fd.revents == 0) {
				continue;
			}

			if (poll_fd.revents & POLLIN) {
				s = read(poll_fd.fd, buf, sizeof(buf));
				if (s < 0) {
					perror("read");
					return EXIT_FAILURE;
				}
				if (buf[0]) // if any button was pressed
				{
					// flash twice
					for (int i = 0; i < 2; ++i) {
						led_turn_off_all(dev);
						usleep(msec_to_usec(200));
						led_turn_on_all(dev);
						usleep(msec_to_usec(200));
						led_turn_off_all(dev);
					}
					return EXIT_SUCCESS;
				}
			} else if (poll_fd.revents & POLLOUT) {
				if (time_in_ms() - last_led_toggle_time_ms < 250) {
					usleep(msec_to_usec(5)); // sleep 5 milliseconds
					break;
				}
			
				if (control_led(dev, led, command)) {
					fprintf(stderr, "Failed to control LED %d\n", led);
					return EXIT_FAILURE;
				}
				last_led_toggle_time_ms = time_in_ms();

				if (led < NUM_LEDS - 1) {
					++led;
				} else {
					led = 0;
					command = !command;
				}
			} else { /* POLLHUP | POLLERR */
				fprintf(stderr, "Recieved error while polling device file\n");
				return EXIT_FAILURE;
			}
		}
	}
}

/*
	Flash the LEDS in the `leds` array to the user
*/
int show_round_leds(struct led_device *dev, int *leds, int round) {
	for (int i = 0; i < round; ++i) {
		if (led_turn_on(dev, leds[i])) {
			return EXIT_FAILURE;
		}
		usleep(msec_to_usec(500));
		if (led_turn_off(dev, leds[i])) {
			return EXIT_FAILURE;
		}
		usleep(msec_to_usec(500));
	}
	return EXIT_SUCCESS;
}

/*
	Read the button input from the user and validate the buttons match the LEDS in the `leds` array.
	If all buttons are correct then return EXIT_SUCCESS, else EXIT_FAILURE.
*/
int check_round(struct led_device *dev, int *leds, int round) {
	size_t s;
	char buf[2];

	for (int i = 0; i < round; ++i) {
		s = fread(buf, sizeof(buf), 1, dev->filp);
		if (s < 0) {
			perror("read");
			exit(EXIT_FAILURE);
		}
		if (atoi(buf) != leds[i]) {
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
};

int main(int argc, char **argv) {
	struct led_device dev;
	int round = 1;
	int *round_leds = NULL;

	// Set a new random seed every time the game is ran
	srand(time(NULL));

	if (led_init_device(&dev) < 0) {
		fprintf(stderr, "[simon] Failed to initialize led device\n");
		exit(EXIT_FAILURE);
	}

	printf("Welcome to Simon! Please press any button to start!\n");
	if (turn_leds_on_off_until_input_loop(&dev)) {
		led_release_device(&dev);
		return EXIT_FAILURE;
	}

	printf("Starting game! Good luck!\n");
	printf("Round: %d", round);
	while (1) {
		// Allocate more buffer space every 5 rounds
		if ((round - 1) % 5 == 0) { 
			round_leds = realloc(round_leds, sizeof(int) * round * 5);
		}
		if (!round_leds) {
			led_release_device(&dev);
			perror("realloc");
			exit(EXIT_FAILURE);
		}

		round_leds[round - 1] = generate_random_number(0, NUM_LEDS - 1);

		if (show_round_leds(&dev, round_leds, round)) {
			free(round_leds);
			led_release_device(&dev);
			fprintf(stdout, "Failed to show LEDS for round %d\n", round);
			exit(EXIT_FAILURE);
		}

		if (check_round(&dev, round_leds, round)) {
			free(round_leds);
			led_release_device(&dev);
			printf("\n");
			printf("You lost!\n");
			return EXIT_SUCCESS;
		}

		++round;
		printf("\b%d", round);
		fflush(stdout);
		usleep(msec_to_usec(1000));
	}

	free(round_leds);
	led_release_device(&dev);

	return EXIT_SUCCESS;
}

