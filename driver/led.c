#include <linux/module.h>
#include <linux/init.h>
#include <linux/mod_devicetable.h>
#include <linux/property.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/gpio/consumer.h>
#include <linux/proc_fs.h>

MODULE_AUTHOR("Neev Cohen");
MODULE_LICENSE("GPL v2");

#define LED_BLUE_INDEX 0
#define LED_RED_INDEX 1

static int dt_probe(struct platform_device *pdev);
static int dt_remove(struct platform_device *pdev);

static struct of_device_id led_ids[] = {
	{
		.compatible = "led",
	}, { /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, led_ids);

static struct platform_driver led_driver = {
	.probe = dt_probe,
	.remove = dt_remove,
	.driver = {
		.name = "led",
		.of_match_table = led_ids,
	},
};

static struct gpio_desc *blue = NULL, *red = NULL;

static struct proc_dir_entry *proc_file = NULL;

char data_buffer[1096];

static ssize_t led_write(struct file *filp, const char *user_buffer, size_t count, loff_t *offset) {
	int led, action;
	struct gpio_desc *gpio;

	memset(data_buffer, 0, sizeof(data_buffer));

	count = min(count, sizeof(data_buffer));

	if (copy_from_user(data_buffer, user_buffer, count)) {
		pr_err("[led] Failed to copy data from user\n");
		return -EFAULT;
	}

	if (sscanf(user_buffer, "%d,%d", &led, &action) != 2) {
		pr_err("[led] Invalid command\n");
		return -EINVAL;
	}

	switch (led) {
		case LED_BLUE_INDEX:
			gpio = blue;
			break;
		case LED_RED_INDEX:
			gpio = red;
			break;
		default:
			return count;
	}

	gpiod_set_value(gpio, !!action);
	return count;
}

static ssize_t led_read(struct file *filp, char __user *user_buffer, size_t count, loff_t *offset) {
	int value;
	char led_value_str;

	if (*offset) {
		return 0;
	}

	value = gpiod_get_raw_value(blue);

	switch (value) {
		case 0:
			led_value_str = '0';
			break;
		case 1:
			led_value_str = '1';
			break;
		default:
			break;
	}
	if (copy_to_user(user_buffer, &led_value_str, 1)) {
		pr_err("[led] Failed to copy led value to user\n");
		return -EFAULT;
	}
	(*offset)++;
	return 1;
};

static struct proc_ops fops = {
	.proc_write = led_write,
	.proc_read = led_read,
};

static int dt_probe(struct platform_device *pdev) {
	struct device *dev = &pdev->dev;

	//blue = gpiod_get(dev, "led-blue", GPIOD_OUT_LOW);
	blue = gpiod_get_index(dev, "led", LED_BLUE_INDEX, GPIOD_OUT_LOW);
	if(IS_ERR(blue)) {
		pr_err("[led] Failed to setup blue LED\n");
		return PTR_ERR(blue);
	}

	//red = gpiod_get(dev, "led-red", GPIOD_OUT_LOW);
	red = gpiod_get_index(dev, "led", LED_RED_INDEX, GPIOD_OUT_LOW);
	if(IS_ERR(red)) {
		printk("[led] Failed to setup red LED\n");
		gpiod_put(blue);
		return PTR_ERR(red);
	}

	proc_file = proc_create("led", 0666, NULL, &fops);
	if(proc_file == NULL) {
		pr_err("[led] Failed to create /proc/led\n");
		gpiod_put(blue);
		gpiod_put(red);
		return -ENOMEM;
	}

	pr_info("[led] Successfully setup GPIO driver\n");

	return 0;
}

static int dt_remove(struct platform_device *pdev) {
	pr_info("[led] Removing device from tree\n");
	gpiod_put(blue);
	blue = NULL;
	gpiod_put(red);
	red = NULL;
	proc_remove(proc_file);
	proc_file = NULL;
	return 0;
}


static int __init led_init(void) {
	pr_info("[led] Loading LED driver\n");
	if (platform_driver_register(&led_driver)) {
		pr_err("[led] Failed to register driver\n");
		return -1;
	}
	return 0;
};

static void __exit led_cleanup(void) {
	pr_info("[led] Cleaning up LED driver\n");
	platform_driver_unregister(&led_driver);
};

module_init(led_init);
module_exit(led_cleanup);

