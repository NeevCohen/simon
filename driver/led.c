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

static struct gpio_desc *blue = NULL;

static struct proc_dir_entry *proc_file = NULL;

static ssize_t led_write(struct file *filp, const char *user_buffer, size_t count, loff_t *offset) {
	int value;
	char command[2];

	if (copy_from_user(command, user_buffer, 1)) {
		pr_err("[led] Failed to read command from user\n");
		return -EFAULT;
	}

	command[1] = 0; // null terminate the string so that kstrtoint doesn't fail

	if (kstrtoint(command, 10, &value)) {
		pr_err("[led] Invalid command from user\n");
		return -EINVAL;
	}

	gpiod_set_value(blue, !!value);
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
	const char *label;
	int ret;

	/* Check for device properties */
	if(!device_property_present(dev, "label")) {
		pr_err("[led] Device property 'label' not found!\n");
		return -1;
	}
	if(!device_property_present(dev, "blue-led-gpio")) {
		pr_err("[led] Device property 'blue-led-gpio' not found!\n");
		return -1;
	}

	/* Read device properties */
	ret = device_property_read_string(dev, "label", &label);
	if(ret) {
		pr_err("[led] Failed to read 'label'\n");
		return -1;
	}
	/* Init GPIO */
	blue = gpiod_get(dev, "blue-led", GPIOD_OUT_LOW);
	if(IS_ERR(blue)) {
		printk("[led] Failed to setup the GPIO\n");
		return -1 * IS_ERR(blue);
	}

	/* Creating procfs file */
	proc_file = proc_create("led", 0666, NULL, &fops);
	if(proc_file == NULL) {
		pr_err("[led] Failed to create /proc/led\n");
		gpiod_put(blue);
		return -ENOMEM;
	}

	pr_info("[led] Successfully setup GPIO driver\n");

	return 0;
}

static int dt_remove(struct platform_device *pdev) {
	pr_info("[led] Removing device from tree\n");
	gpiod_put(blue);
	proc_remove(proc_file);
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

