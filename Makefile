ccflags-y += -Wall -O2 -DDBG=0 -D_LINUX -D_KERNEL -Wframe-larger-than=1064
ccflags-y += -I$(PWD)/include
DRVPATH := /lib/modules/$(shell uname -r)/kernel/drivers
DEPMOD := $(shell which depmod)
ccflags-y += -I$(PWD)/include

# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE), )

	obj-m := drivers/mfd/eiois200_core.o
	obj-m += drivers/watchdog/eiois200_wdt.o
	obj-m += drivers/hwmon/eiois200-hwmon.o
	obj-m += drivers/thermal/eiois200_thermal.o
	obj-m += drivers/thermal/eiois200_fan.o
	obj-m += drivers/video/backlight/eiois200_bl.o
	obj-m += drivers/gpio/gpio-eiois200.o
	obj-m += drivers/i2c/busses/i2c-eiois200.o
    
else
# Otherwise we were called directly from the command
# line; invoke the kernel build system.
	KDIR := /lib/modules/$(shell uname -r)/build

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	
install:
	install -D -m 644 drivers/mfd/eiois200_core.ko $(DRVPATH)/mfd/eiois200_core.ko
	install -D -m 644 drivers/watchdog/eiois200_wdt.ko $(DRVPATH)/watchdog/eiois200_wdt.ko
	install -D -m 644 drivers/hwmon/eiois200-hwmon.ko $(DRVPATH)/hwmon/eiois200-hwmon.ko
#	install -D -m 644 drivers/thermal/eiois200_thermal.ko $(DRVPATH)/thermal/eiois200_thermal.ko
	install -D -m 644 drivers/thermal/eiois200_fan.ko $(DRVPATH)/thermal/eiois200_fan.ko
	install -D -m 644 drivers/video/backlight/eiois200_bl.ko $(DRVPATH)/video/backlight/eiois200_bl.ko
	install -D -m 644 drivers/gpio/gpio-eiois200.ko $(DRVPATH)/gpio/gpio-eiois200.ko
	install -D -m 644 drivers/i2c/busses/i2c-eiois200.ko $(DRVPATH)/i2c/busses/i2c-eiois200.ko

	$(DEPMOD) -a

uninstall:
	- rm -rf $(DRVPATH)/mfd/eiois200_core.ko
	- rm -rf $(DRVPATH)/watchdog/eiois200_wdt.ko
	- rm -rf $(DRVPATH)/hwmon/eiois200-hwmon.ko
#	- rm -rf $(DRVPATH)/thermal/eiois200_thermal.ko
	- rm -rf $(DRVPATH)/thermal/eiois200_fan.ko
	- rm -rf $(DRVPATH)/video/backlight/eiois200_bl.ko
	- rm -rf $(DRVPATH)/gpio/gpio-eiois200_wdt.ko
	- rm -rf $(DRVPATH)/i2c/busses/i2c-eiois200_wdt.ko
	
	- $(DEPMOD) -a
	
load:
	- modprobe eiois200_core
	- modprobe eiois200_wdt
	- modprobe gpio-eiois200
	- modprobe eiois200-hwmon
	- modprobe eiois200_fan
#	- modprobe eiois200_thermal
	- modprobe i2c-eiois200
	- modprobe eiois200_bl

unload:
	- rmmod gpio-eiois200
	- rmmod i2c-eiois200
	- rmmod eiois200_wdt
	- rmmod eiois200_hwmon
	- rmmod eiois200_fan
#	- rmmod eiois200_thermal
	- rmmod eiois200_bl
	- rmmod eiois200_core
	
all:  default unload uninstall install load

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

endif
