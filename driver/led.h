#include <linux/gpio/consumer.h>

#define DATA_BUFFER_SIZE 1096

struct led_device {
	struct gpio_desc *blue, 
			  *red, 
			  *green, 
			  *yellow, 
			  *btn_0, 
			  *btn_1, 
			  *btn_2, 
			  *btn_3;
	struct proc_dir_entry *proc_file;
	char data_buffer[DATA_BUFFER_SIZE];
};

