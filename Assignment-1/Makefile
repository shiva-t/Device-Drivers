obj-m := main.o

KDIR = /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	rm -rf *.order *.symvers *.mod.o *.o *.mod.c
