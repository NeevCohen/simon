#include <linux/module.h>
#include <linux/init.h>
#include <linux/mod_devicetable.h>
#include <linux/property.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/jiffies.h>
extern unsigned long volatile jiffies;

#include "led.h"
MODULE_AUTHOR("Neev Cohen");
MODULE_LICENSE("GPL v2");

static struct led_device led_dev = {
	.led_0 = NULL,
	.led_1 = NULL,
	.led_2 = NULL,
	.led_3 = NULL,
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
	.read_buffer = { 0, 0 },
	.nreaders = 0,
	.nwriters = 0,
};

#define BUTTON_BOUNCE_MS 200

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

	if (jiffies_to_msecs(jiffies - *last_press_jiffies) < BUTTON_BOUNCE_MS) // bounce
	{
		return IRQ_HANDLED;
	}

	*last_press_jiffies = jiffies;

	return IRQ_WAKE_THREAD;
};

static irqreturn_t btn_pushed_thread_func(int irq, void *dev_id) {
	char button_pushed;

	if (irq == led_dev.irq_btn_0) {
		button_pushed = '0';
	} 
	else if (irq == led_dev.irq_btn_1) {
		button_pushed = '1';
	} 
	else if (irq == led_dev.irq_btn_2) {
		button_pushed = '2';
	} 
	else if (irq == led_dev.irq_btn_3) {
		button_pushed = '3';
	}

	if (led_dev.nreaders) {
		led_dev.read_buffer[0] = button_pushed;
		wake_up_interruptible(&led_dev.read_queue);
	}

	return IRQ_HANDLED;
};

static int led_open(struct inode *inode, struct file *filp) {
	filp->private_data = &led_dev;

	if (mutex_lock_interruptible(&led_dev.lock))
		return -ERESTARTSYS;

	/* use f_mode,not  f_flags: it's cleaner (fs/open.c tells why) */
	if (filp->f_mode & FMODE_READ)
		led_dev.nreaders++;
	if (filp->f_mode & FMODE_WRITE)
		led_dev.nwriters++;
	mutex_unlock(&led_dev.lock);
	return 0;
};

static int led_release(struct inode *inode, struct file *filp) {
	struct led_device *dev = filp->private_data;

	mutex_lock(&dev->lock);
	if (filp->f_mode & FMODE_READ)
		dev->nreaders--;
	if (filp->f_mode & FMODE_WRITE)
		dev->nwriters--;
	mutex_unlock(&dev->lock);
	return 0;
};

static ssize_t led_read(struct file *filp, char __user *user_buffer, size_t count, loff_t *offset) {
	struct led_device *dev = filp->private_data;

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	while (dev->read_buffer[0] == 0) {
		mutex_unlock(&dev->lock);
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		if (wait_event_interruptible(dev->read_queue, dev->read_buffer[0] != 0))
			return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
		/* otherwise loop, but first reacquire the lock */
		if (mutex_lock_interruptible(&dev->lock))
			return -ERESTARTSYS;
	}

	if (count > 2) {
		count = 2;
	}

	if (copy_to_user(user_buffer, dev->read_buffer, count)) {
		pr_err("[led] Failed to copy data to user\n");
		mutex_unlock(&dev->lock);
		return -EFAULT;
	}

	dev->read_buffer[0] = 0;
	mutex_unlock(&dev->lock);

	return count;
};

static ssize_t led_write(struct file *filp, const char *user_buffer, size_t count, loff_t *offset) {
	struct led_device *dev = filp->private_data;
	int led, action;
	struct gpio_desc *gpio;

	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	while (dev->nwriters > 1) {
		mutex_unlock(&dev->lock);
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		if (wait_event_interruptible(dev->write_queue, dev->nwriters > 1))
			return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
		/* otherwise loop, but first reacquire the lock */
		if (mutex_lock_interruptible(&dev->lock))
			return -ERESTARTSYS;
	}
	memset(dev->write_buffer, 0, sizeof(dev->write_buffer));
	count = min(count, sizeof(dev->write_buffer));

	if (copy_from_user(dev->write_buffer, user_buffer, count))
	{
		pr_err("[led] Failed to copy data from user\n");
		mutex_unlock(&dev->lock);
		return -EFAULT;
	}

	if (sscanf(dev->write_buffer, "%d,%d", &led, &action) != 2)
	{
		pr_err("[led] Invalid command\n");
		mutex_unlock(&dev->lock);
		return -EINVAL;
	}

	switch (led) {
		case 0:
			gpio = dev->led_0;
			break;
		case 1:
			gpio = dev->led_1;
			break;
		case 2:
			gpio = dev->led_2;
			break;
		case 3:
			gpio = dev->led_3;
			break;
		default:
			return count;
	}

	gpiod_set_value(gpio, !!action);
	mutex_unlock(&dev->lock);
	return count;
}

static unsigned int led_poll(struct file *filp, poll_table *wait) {
	struct led_device *dev = filp->private_data;
	// Device is always writeable
	unsigned int mask = POLLOUT | POLLWRNORM;

	mutex_lock(&dev->lock);
	poll_wait(filp, &dev->read_queue, wait);
	if (dev->nreaders > 0 && dev->read_buffer[0]) { // if there are readers and data to read
		mask |= POLLIN | POLLRDNORM;
	}
	mutex_unlock(&dev->lock);
	return mask;
};

static struct proc_ops fops = {
	.proc_open = led_open,
	.proc_release = led_release,
	.proc_read = led_read,
	.proc_write = led_write,
	.proc_poll = led_poll,
};

static int setup_leds_gpios(struct device *dev) {
	int ret = 0;

	led_dev.led_0 = gpiod_get_index(dev, "led", 0, GPIOD_OUT_LOW);
	if (IS_ERR(led_dev.led_0)) 
	{
		pr_err("[led] Failed to setup LED 0\n");
		return PTR_ERR(led_dev.led_0);
	}

	led_dev.led_1 = gpiod_get_index(dev, "led", 1, GPIOD_OUT_LOW);
	if (IS_ERR(led_dev.led_1)) 
	{
		pr_err("[led] Failed to setup LED 1\n");
		ret = PTR_ERR(led_dev.led_1);
		goto error_led_1;
	}

	led_dev.led_2 = gpiod_get_index(dev, "led", 2, GPIOD_OUT_LOW);
	if (IS_ERR(led_dev.led_2)) 
	{
		pr_err("[led] Failed to setup LED 2\n");
		ret = PTR_ERR(led_dev.led_2);
		goto error_led_2;
	}

	led_dev.led_3 = gpiod_get_index(dev, "led", 3, GPIOD_OUT_LOW);
	if (IS_ERR(led_dev.led_3)) 
	{
		pr_err("[led] Failed to setup LED 3\n");
		ret = PTR_ERR(led_dev.led_3);
		goto error_led_3;
	}

	return ret;	

error_led_3:
	gpiod_put(led_dev.led_2);
error_led_2:
	gpiod_put(led_dev.led_1);
error_led_1:
	gpiod_put(led_dev.led_0);
	return ret;
};

static int setup_btns_gpios(struct device *dev) {
	int ret = 0;

	led_dev.btn_0 = gpiod_get_index(dev, "btn", 0, GPIOD_IN);
	if (IS_ERR(led_dev.btn_0)) 
	{
		pr_err("[led] Failed to setup button 0\n");
		return PTR_ERR(led_dev.btn_0);
	}

	led_dev.btn_1 = gpiod_get_index(dev, "btn", 1, GPIOD_IN);
	if (IS_ERR(led_dev.btn_1)) 
	{
		printk("[led] Failed to setup button 1\n");
		ret = PTR_ERR(led_dev.btn_1);
		goto error_btn_1;
	}

	led_dev.btn_2 = gpiod_get_index(dev, "btn", 2, GPIOD_IN);
	if (IS_ERR(led_dev.btn_2)) 
	{
		printk("[led] Failed to setup button 2\n");
		ret = PTR_ERR(led_dev.btn_2);
		goto error_btn_2;
	}

	led_dev.btn_3 = gpiod_get_index(dev, "btn", 3, GPIOD_IN);
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
	if (led_dev.led_0) 
	{
		gpiod_put(led_dev.led_0);
		led_dev.led_0 = NULL;
	}

	if (led_dev.led_1) 
	{
		gpiod_put(led_dev.led_1);
		led_dev.led_1 = NULL;
	}

	if (led_dev.led_2) 
	{
		gpiod_put(led_dev.led_2);
		led_dev.led_2 = NULL;
	}

	if (led_dev.led_3) 
	{
		gpiod_put(led_dev.led_3);
		led_dev.led_3 = NULL;
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

	init_waitqueue_head(&led_dev.read_queue);
	init_waitqueue_head(&led_dev.write_queue);
	mutex_init(&led_dev.lock);
	
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

