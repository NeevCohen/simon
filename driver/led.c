#include <linux/module.h>
#include <linux/init.h>
#include <linux/mod_devicetable.h>
#include <linux/property.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>

#include <linux/jiffies.h>
extern unsigned long volatile jiffies;

#include "led.h"

MODULE_AUTHOR("Neev Cohen");
MODULE_LICENSE("GPL v2");

static struct led_device led_dev = {
	.blue = NULL,
	.red = NULL,
	.green = NULL,
	.yellow = NULL,
	.btn_0 = NULL,
	.btn_1 = NULL,
	.btn_2 = NULL,
	.btn_3 = NULL,
	.irq_btn_0 = 0,
	.irq_btn_1 = 0,
	.irq_btn_2 = 0,
	.irq_btn_3 = 0,
	.btn_0_last_press_jiffies = 0,
	.btn_1_last_press_jiffies = 0,
	.btn_2_last_press_jiffies = 0,
	.btn_3_last_press_jiffies = 0,
	.proc_file = NULL,
};

#define LED_BLUE_INDEX 0
#define LED_RED_INDEX 1
#define LED_GREEN_INDEX 2
#define LED_YELLOW_INDEX 3

#define BTN_0_INDEX 0
#define BTN_1_INDEX 1
#define BTN_2_INDEX 2
#define BTN_3_INDEX 3

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

static irqreturn_t handle_irq(int irq, void *dev_id) {
	unsigned long *last_press_jiffies;

	if (irq == led_dev.irq_btn_0) 
	{
		last_press_jiffies = &led_dev.btn_0_last_press_jiffies;
	} 
	else if (irq == led_dev.irq_btn_1) 
	{
		last_press_jiffies = &led_dev.btn_1_last_press_jiffies;
	} 
	else if (irq == led_dev.irq_btn_2) 
	{
		last_press_jiffies = &led_dev.btn_2_last_press_jiffies;
	} 
	else if (irq == led_dev.irq_btn_3) 
	{
		last_press_jiffies = &led_dev.btn_3_last_press_jiffies;
	}

	if (jiffies_to_msecs(jiffies - *last_press_jiffies) < 200) // bounce
	{
		return IRQ_HANDLED;
	}

	*last_press_jiffies = jiffies;

	return IRQ_WAKE_THREAD;
};

static irqreturn_t btn_pushed_thread_func(int irq, void *dev_id) {
	if (irq == led_dev.irq_btn_0) 
	{
		pr_info("Button 0 was pushed\n");
	} 
	else if (irq == led_dev.irq_btn_1) 
	{
		pr_info("Button 1 was pushed\n");
	} 
	else if (irq == led_dev.irq_btn_2) 
	{
		pr_info("Button 2 was pushed\n");
	} 
	else if (irq == led_dev.irq_btn_3) 
	{
		pr_info("Button 3 was pushed\n");
	}

	return IRQ_HANDLED;
};

static ssize_t led_write(struct file *filp, const char *user_buffer, size_t count, loff_t *offset) {
	int led, action;
	struct gpio_desc *gpio;

	memset(led_dev.data_buffer, 0, sizeof(led_dev.data_buffer));

	count = min(count, sizeof(led_dev.data_buffer));

	if (copy_from_user(led_dev.data_buffer, user_buffer, count)) 
	{
		pr_err("[led] Failed to copy data from user\n");
		return -EFAULT;
	}

	if (sscanf(led_dev.data_buffer, "%d,%d", &led, &action) != 2) 
	{
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

static int setup_leds_gpios(struct device *dev) {
	int ret = 0;

	led_dev.blue = gpiod_get_index(dev, "led", LED_BLUE_INDEX, GPIOD_OUT_LOW);
	if (IS_ERR(led_dev.blue)) 
	{
		pr_err("[led] Failed to setup blue LED\n");
		return PTR_ERR(led_dev.blue);
	}

	led_dev.red = gpiod_get_index(dev, "led", LED_RED_INDEX, GPIOD_OUT_LOW);
	if (IS_ERR(led_dev.red)) 
	{
		pr_err("[led] Failed to setup red LED\n");
		ret = PTR_ERR(led_dev.red);
		goto error_red;
	}

	led_dev.green = gpiod_get_index(dev, "led", LED_GREEN_INDEX, GPIOD_OUT_LOW);
	if (IS_ERR(led_dev.green)) 
	{
		pr_err("[led] Failed to setup green LED\n");
		ret = PTR_ERR(led_dev.green);
		goto error_green;
	}

	led_dev.yellow = gpiod_get_index(dev, "led", LED_YELLOW_INDEX, GPIOD_OUT_LOW);
	if (IS_ERR(led_dev.yellow)) 
	{
		pr_err("[led] Failed to setup yellow LED\n");
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

static int setup_btns_gpios(struct device *dev) {
	int ret = 0;

	led_dev.btn_0 = gpiod_get_index(dev, "btn", BTN_0_INDEX, GPIOD_IN);
	if (IS_ERR(led_dev.btn_0)) 
	{
		pr_err("[led] Failed to setup button 0\n");
		return PTR_ERR(led_dev.btn_0);
	}

	led_dev.btn_1 = gpiod_get_index(dev, "btn", BTN_1_INDEX, GPIOD_IN);
	if (IS_ERR(led_dev.btn_1)) 
	{
		printk("[led] Failed to setup button 1\n");
		ret = PTR_ERR(led_dev.btn_1);
		goto error_btn_1;
	}

	led_dev.btn_2 = gpiod_get_index(dev, "btn", BTN_2_INDEX, GPIOD_IN);
	if (IS_ERR(led_dev.btn_2)) 
	{
		printk("[led] Failed to setup button 2\n");
		ret = PTR_ERR(led_dev.btn_2);
		goto error_btn_2;
	}

	led_dev.btn_3 = gpiod_get_index(dev, "btn", BTN_3_INDEX, GPIOD_IN);
	if (IS_ERR(led_dev.btn_3)) 
	{
		printk("[led] Failed to setup button 3\n");
		ret = PTR_ERR(led_dev.btn_3);
		goto error_btn_3;
	}

	return ret;

error_btn_3:
	gpiod_put(led_dev.btn_2);
error_btn_2:
	gpiod_put(led_dev.btn_1);
error_btn_1:
	gpiod_put(led_dev.btn_0);
	return ret;
};

static int setup_btns_irqs(void) {
	int ret = 0;

	led_dev.irq_btn_0 = gpiod_to_irq(led_dev.btn_0);
	ret = request_threaded_irq(led_dev.irq_btn_0,
				   handle_irq,
				   btn_pushed_thread_func,
				   IRQF_TRIGGER_RISING,
				   "led-btn-0", 
				   NULL);
	if (ret) 
	{
		pr_err("[led] Failed to register IRQ for button 0\n");
		return ret;
	}

	led_dev.irq_btn_1 = gpiod_to_irq(led_dev.btn_1);
	ret = request_threaded_irq(led_dev.irq_btn_1,
				   handle_irq,
				   btn_pushed_thread_func,
				   IRQF_TRIGGER_RISING,
				   "led-btn-1", 
				   NULL);
	if (ret) 
	{
		pr_err("[led] Failed to register IRQ for button 1\n");
		goto btn_1_irq_error;
	}

	led_dev.irq_btn_2 = gpiod_to_irq(led_dev.btn_2);
	ret = request_threaded_irq(led_dev.irq_btn_2,
				   handle_irq,
				   btn_pushed_thread_func,
				   IRQF_TRIGGER_RISING,
				   "led-btn-3", 
				   NULL);
	if (ret) 
	{
		pr_err("[led] Failed to register IRQ for button 2\n");
		goto btn_2_irq_error;
	}

	led_dev.irq_btn_3 = gpiod_to_irq(led_dev.btn_3);
	ret = request_threaded_irq(led_dev.irq_btn_3,
				   handle_irq,
				   btn_pushed_thread_func,
				   IRQF_TRIGGER_RISING,
				   "led-btn-3", 
				   NULL);
	if (ret) 
	{
		pr_err("[led] Failed to register IRQ for button 3\n");
		goto btn_3_irq_error;
	}

	return ret;

btn_3_irq_error:
	free_irq(led_dev.irq_btn_2, NULL);
btn_2_irq_error:
	free_irq(led_dev.irq_btn_1, NULL);
btn_1_irq_error:
	free_irq(led_dev.irq_btn_0, NULL);
	return ret;
};

static void free_btns_gpios(void) {
	if (led_dev.btn_0) 
	{
		gpiod_put(led_dev.btn_0);
		led_dev.btn_0 = NULL;
	}

	if (led_dev.btn_1) 
	{
		gpiod_put(led_dev.btn_1);
		led_dev.btn_1 = NULL;
	}

	if (led_dev.btn_2) 
	{
		gpiod_put(led_dev.btn_2);
		led_dev.btn_2 = NULL;
	}

	if (led_dev.btn_3) 
	{
		gpiod_put(led_dev.btn_3);
		led_dev.btn_3 = NULL;
	}
};

static void free_leds_gpios(void) {
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
};

static void free_btns_irqs(void) {
	if (led_dev.irq_btn_0) 
	{
		free_irq(led_dev.irq_btn_0, NULL);
		led_dev.irq_btn_0 = 0;
	}

	if (led_dev.irq_btn_1) 
	{
		free_irq(led_dev.irq_btn_1, NULL);
		led_dev.irq_btn_1 = 0;
	}

	if (led_dev.irq_btn_2) 
	{
		free_irq(led_dev.irq_btn_2, NULL);
		led_dev.irq_btn_2 = 0;
	}

	if (led_dev.irq_btn_3) 
	{
		free_irq(led_dev.irq_btn_3, NULL);
		led_dev.irq_btn_3 = 0;
	}
};

static int dt_probe(struct platform_device *pdev) {
	struct device *dev = &pdev->dev;
	int ret = 0;
	
	if ((ret = setup_leds_gpios(dev))) 
	{
		return ret;
	}

	if ((ret = setup_btns_gpios(dev))) 
	{
		goto error_btns;
	}

	if ((ret = setup_btns_irqs())) 
	{
		goto error_btn_irqs;
	}
	
	led_dev.proc_file = proc_create("led", 0666, NULL, &fops);
	if (led_dev.proc_file == NULL) 
	{
		pr_err("[led] Failed to create /proc/led\n");
		ret = -ENOMEM;
		goto error_proc_file;
	}

	pr_info("[led] Successfully setup GPIO driver\n");

	return ret;

error_proc_file:
	free_btns_irqs();
error_btn_irqs:
	free_btns_gpios();
error_btns:
	free_leds_gpios();

	return ret;
};

static int dt_remove(struct platform_device *pdev) {
	pr_info("[led] Removing device\n");

	free_btns_irqs();
	free_btns_gpios();
	free_leds_gpios();
	proc_remove(led_dev.proc_file);

	return 0;
};

static int __init led_init(void) {
	pr_info("[led] Loading LED driver\n");
	if (platform_driver_register(&led_driver)) 
	{
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

