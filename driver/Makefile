obj-m += led.o

KDIR = /lib/modules/$(shell uname -r)/build

all: module dt
	echo Built Device Tree Overlay and kernel module

module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
dt: ledoverlay.dts
	dtc -@ -O dtb -o ledoverlay.dtbo ledoverlay.dts
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -rf ledoverlay.dtbo

