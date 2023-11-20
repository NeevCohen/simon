#include <linux/module.h>

MODULE_AUTHOR("Neev Cohen");
MODULE_LICENSE("GPL v2");

static int __init simon_init(void) {
	pr_info("[simon] Hello world from Simon!\n");
	return 0;
};

static void __exit simon_cleanup(void) {
	pr_info("[simon] Goodbye world from Simon!\n");
};

module_init(simon_init);
module_exit(simon_cleanup);

