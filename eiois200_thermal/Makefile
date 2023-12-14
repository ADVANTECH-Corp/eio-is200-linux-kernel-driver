#!/usr/bin/make -f

MODULE_NAME := eiois200_thermal

KVER := $(shell uname -r)
KERNEL_MODLIB := /lib/modules/$(KVER)
MODDIR := $(KERNEL_MODLIB)/extra
INCLUDEDIR := /usr/local/include
KDIR := /lib/modules/$(KVER)/build
PWD := $(shell pwd)
KBUILD_EXTRA_SYMBOLS := $(src)/../eiois200_core/Module.symvers

obj-m := $(MODULE_NAME).o

src ?= $(PWD)
obj ?= .
ccflags-y := -I$(src)/../include

module: $(MODULE_NAME).ko

$(MODULE_NAME).ko: $(MODULE_NAME).c
	$(MAKE) -C "$(KDIR)" M="$(src)" modules

$(MODULE_NAME).mod.c: $(MODULE_NAME).ko
$(MODULE_NAME).mod.o: $(MODULE_NAME).mod.c

clean:
	rm $(obj-m) $(MODULE_NAME).mod.c $(MODULE_NAME).mod.o $(MODULE_NAME).ko modules.order .$(MODULE_NAME)*.cmd $(MODULE_NAME).mod Module.symvers || true

install: module $(MODULE_NAME).mod.o
	install -d "$(MODDIR)"
	install -m 644 $(MODULE_NAME).ko "$(MODDIR)/"
	depmod "$(KVER)"

uninstall:
	rm "$(MODDIR)"/$(MODULE_NAME).ko || true
	rmdir --ignore-fail-on-non-empty "$(MODDIR)"
	depmod "$(KVER)"

load: module
	modprobe $(MODULE_NAME)

unload:
	! grep -q "^$(MODULE_NAME) " /proc/modules || rmmod $(MODULE_NAME)

.PHONY: module clean install uninstall load unload install-dkms uninstall-dkms

