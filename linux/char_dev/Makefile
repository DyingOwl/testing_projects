
#Makefile for the gpio_irq.c
SHELL = /bin/bash

obj-m := char_dev.o
CURRENT_PATH := $(shell pwd)
LINUX_KERNEL_PATH := /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) $(CFLAGS) -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) modules

clean:
	rm *.ko
	rm *.o
