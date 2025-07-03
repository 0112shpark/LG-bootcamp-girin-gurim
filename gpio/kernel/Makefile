obj-m := device_Control.o

KERNELDIR := ~/work/new-kernel
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
	cp device_Control.ko /nfsroot

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean