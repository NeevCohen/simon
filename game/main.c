#include <sys/time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <poll.h>

#include "led.h"

#define USEC_IN_MSEC 1000

static const char start_command = 's';

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
	return rand() % (max + 1 - max) + min;
}

/*
Run in loop, turning all leds on and off until a user enters the start command
*/
int turn_leds_on_off_until_input_loop(struct led_device *dev) {
	int led = 0, command = ON;
	struct pollfd *poll_fds;
	nfds_t nfds = 2;
	size_t s;
	char buf[2];
	long long last_led_toggle_time_ms = time_in_ms();

	poll_fds = calloc(nfds, sizeof(struct pollfd));
	if (!poll_fds) {
		perror("calloc");
		return EXIT_FAILURE;
	}

	poll_fds[0].fd = fileno(stdin);
	poll_fds[0].events = POLLIN;

	poll_fds[1].fd = fileno(dev->filp);
	poll_fds[1].events = POLLOUT;

	while (1) {
		if (poll(poll_fds, nfds, -1) < 0) {
			perror("poll");
			free(poll_fds);
			return EXIT_FAILURE;
		}

		for (nfds_t i = 0; i < nfds; ++i) {
			if (poll_fds[i].revents == 0) {
				continue;
			}

			if (poll_fds[i].revents & POLLIN) {
				s = read(poll_fds[i].fd, buf, sizeof(buf));
				if (s < 0) {
					led_release_device(dev);
					perror("read");
					free(poll_fds);
					return EXIT_FAILURE;
				}
				if (buf[0] == start_command) {
					// flash twice
					for (int i = 0; i < 2; ++i) {
						led_turn_off_all(dev);
						usleep(msec_to_usec(200));
						led_turn_on_all(dev);
						usleep(msec_to_usec(200));
						led_turn_off_all(dev);
					}
					free(poll_fds);
					return EXIT_SUCCESS;
				}
				fflush(stdin);
			} else if (poll_fds[i].revents & POLLOUT) {
				if (time_in_ms() - last_led_toggle_time_ms < 250) {
					usleep(msec_to_usec(5)); // sleep 5 milliseconds
					break;
				}
			
				if (control_led(dev, led, command)) {
					fprintf(stderr, "Failed to control LED %d\n", led);
					free(poll_fds);
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
				printf("Failed %d %d\n", i, poll_fds[i].revents & POLLHUP);
				free(poll_fds);
				return EXIT_FAILURE;
			}
		}
	}
}

int main(int argc, char **argv) {
	struct led_device dev;
	struct pollfd poll_fd;
	int i, round;
	size_t s;
	char buf[2];

	if (led_init_device(&dev) < 0) {
		fprintf(stderr, "[simon] Failed to initialize led device\n");
		exit(EXIT_FAILURE);
	}

	printf("Welcome to Simon! Please press '%c' to start!\n", start_command);
	if (turn_leds_on_off_until_input_loop(&dev)) {
		led_release_device(&dev);
		return EXIT_FAILURE;
	}
	printf("Starting game! Good luck!\n");
	poll_fd.fd = fileno(dev.filp);
	poll_fd.events = POLLIN;

	while (1) {
		if (poll(&poll_fd, 1, -1) < 0) {
			led_release_device(&dev);
			perror("poll");
			exit(EXIT_FAILURE);
		}

		printf("Round: %d\n", round);
		if (poll_fd.revents & POLLIN) {
			s = fread(buf, sizeof(buf), 1, dev.filp);
			if (s < 0) {
				led_release_device(&dev);
				perror("read");
				exit(EXIT_FAILURE);
			}
			printf("Nice!\n");
			round++;
		} else {
			led_release_device(&dev);
			fprintf(stderr, "Failed to poll\n");
			exit(EXIT_FAILURE);
		}
	}

	led_release_device(&dev);

	return EXIT_SUCCESS;
}

