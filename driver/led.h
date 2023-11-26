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
	int irq_btn_0, 
	    irq_btn_1, 
	    irq_btn_2, 
	    irq_btn_3;
	unsigned long btn_0_last_press_jiffies,
		      btn_1_last_press_jiffies,
		      btn_2_last_press_jiffies,
		      btn_3_last_press_jiffies;
	struct proc_dir_entry *proc_file;
	char data_buffer[DATA_BUFFER_SIZE];
};

