#
# Makefile for the Linux minix filesystem routines.
#

# TODO: adapt for new file chown.c

KERNELDIR=/lib/modules/`uname -r`/build
#ARCH=i386
#KERNELDIR=/usr/src/kernels/`uname -r`-i686

MODULES = firewallExtension.ko

obj-m += firewallExtension.o

firewallExtension-objs := firewallExtension_h.o frule.o 

all:
	make -C  $(KERNELDIR) M=$(PWD) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean

install:	
	make -C $(KERNELDIR) M=$(PWD) modules_install
