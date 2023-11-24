#include <linux/module.h>
#include <linux/init.h>
#include <linux/mod_devicetable.h>
#include <linux/property.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/proc_fs.h>

#include "led.h"

MODULE_AUTHOR("Neev Cohen");
MODULE_LICENSE("GPL v2");

static struct led_device led_dev;

#define LED_BLUE_INDEX 0
#define LED_RED_INDEX 1
#define LED_GREEN_INDEX 2
#define LED_YELLOW_INDEX 3

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

static ssize_t led_write(struct file *filp, const char *user_buffer, size_t count, loff_t *offset) {
	int led, action;
	struct gpio_desc *gpio;

	memset(led_dev.data_buffer, 0, sizeof(led_dev.data_buffer));

	count = min(count, sizeof(led_dev.data_buffer));

	if (copy_from_user(led_dev.data_buffer, user_buffer, count)) {
		pr_err("[led] Failed to copy data from user\n");
		return -EFAULT;
	}

	if (sscanf(led_dev.data_buffer, "%d,%d", &led, &action) != 2) {
		pr_err("[led] Invalid command\n");
		return -EINVAL;
	}

	switch (led) {
		case LED_BLUE_INDEX:
			gpio = led_dev.blue;
			break;
		case LED_RED_INDEX:
			gpio = led_dev.red;
			break;
		case LED_GREEN_INDEX:
			gpio = led_dev.green;
			break;
		case LED_YELLOW_INDEX:
			gpio = led_dev.yellow;
			break;
		default:
			return count;
	}

	gpiod_set_value(gpio, !!action);
	return count;
}

static ssize_t led_read(struct file *filp, char __user *user_buffer, size_t count, loff_t *offset) {
	return 0;
};

static struct proc_ops fops = {
	.proc_write = led_write,
	.proc_read = led_read,
};

static int led_setup_gpio(struct device *dev) {
	int ret = 0;

	led_dev.blue = gpiod_get_index(dev, "led", LED_BLUE_INDEX, GPIOD_OUT_LOW);
	if(IS_ERR(led_dev.blue)) {
		pr_err("[led] Failed to setup blue LED\n");
		return PTR_ERR(led_dev.blue);
	}

	led_dev.red = gpiod_get_index(dev, "led", LED_RED_INDEX, GPIOD_OUT_LOW);
	if(IS_ERR(led_dev.red)) {
		printk("[led] Failed to setup red LED\n");
		ret = PTR_ERR(led_dev.red);
		goto error_red;
	}

	led_dev.green = gpiod_get_index(dev, "led", LED_GREEN_INDEX, GPIOD_OUT_LOW);
	if(IS_ERR(led_dev.green)) {
		printk("[led] Failed to setup green LED\n");
		ret = PTR_ERR(led_dev.green);
		goto error_green;
	}

	led_dev.yellow = gpiod_get_index(dev, "led", LED_YELLOW_INDEX, GPIOD_OUT_LOW);
	if(IS_ERR(led_dev.yellow)) {
		printk("[led] Failed to setup yellow LED\n");
		ret = PTR_ERR(led_dev.yellow);
		goto error_yellow;
	}

	return ret;	

error_yellow:
	gpiod_put(led_dev.green);
error_green:
	gpiod_put(led_dev.red);
error_red:
	gpiod_put(led_dev.blue);
	return ret;
};

static int dt_probe(struct platform_device *pdev) {
	struct device *dev = &pdev->dev;
	int ret = 0;
	
	if ((ret = led_setup_gpio(dev))) {
		return ret;
	}
	
	led_dev.proc_file = proc_create("led", 0666, NULL, &fops);
	if (led_dev.proc_file == NULL) 
	{
		pr_err("[led] Failed to create /proc/led\n");
		return -ENOMEM;
	}

	pr_info("[led] Successfully setup GPIO driver\n");

	return ret;
}

static int dt_remove(struct platform_device *pdev) {
	pr_info("[led] Removing device from tree\n");
	if (led_dev.blue) 
	{
		gpiod_put(led_dev.blue);
		led_dev.blue = NULL;
	}

	if (led_dev.red) 
	{
		gpiod_put(led_dev.red);
		led_dev.red = NULL;
	}

	if (led_dev.green) 
	{
		gpiod_put(led_dev.green);
		led_dev.green = NULL;
	}

	if (led_dev.yellow) 
	{
		gpiod_put(led_dev.yellow);
		led_dev.yellow = NULL;
	}

	if (led_dev.proc_file) 
	{
		proc_remove(led_dev.proc_file);
		led_dev.proc_file = NULL;
	}

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

