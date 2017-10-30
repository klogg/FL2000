# Makefile
#
# (c)Copyright 2017, Fresco Logic, Incorporated.
#
# The contents of this file are property of Fresco Logic, Incorporated and are strictly protected
# by Non Disclosure Agreements. Distribution in any form to unauthorized parties is strictly prohibited.
#
# Purpose:
#
# NOTE: DO NOT SEND THIS FILE OUTSIDE OF FRESCO LOGIC.
#

fl2000-y := src/fl2000_module.o \
	    src/fl2000_bulk.o \
	    src/fl2000_ioctl.o \
	    src/fl2000_render.o \
	    src/fl2000_dev.o \
	    src/fl2000_dongle.o \
	    src/fl2000_big_table.o \
	    src/fl2000_i2c.o \
	    src/fl2000_register.o \
	    src/fl2000_monitor.o \
	    src/fl2000_desc.o \
	    src/fl2000_interrupt.o \
	    src/fl2000_compression.o \
	    src/fl2000_surface.o \
	    src/fl2000_fops.o \
	    src/fl2000_hdmi.o \

ifdef CONFIG_USB_FL2000

obj-$(CONFIG_USB_FL2000) := fl2000.o

else

obj-m := fl2000.o

KSRC = /lib/modules/$(shell uname -r)/build

all:	modules

modules:
	make -C $(KSRC) M=$(PWD) modules

clean:
	make -C $(KSRC) M=$(PWD) clean
	rm -f Module.symvers

endif


