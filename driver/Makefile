obj-m += simon.o

KDIR = /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) M=$(shell pwd) modules
	rm -rf *.symvers *.mod.o *.mod *.mod.c *.o *.order

clean:
	make -C $(KDIR) M=$(shell pwd) clean

