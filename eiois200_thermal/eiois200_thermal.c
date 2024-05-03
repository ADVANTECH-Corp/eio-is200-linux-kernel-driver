// SPDX-License-Identifier: GPL-2.0-only
/*
 * eiois200_thermal
 * ================
 * Thermal zone driver for Advantech EIO-IS200 embedded controller's thermal
 * protect mechanism.
 *
 * The driver load the EC current thermal protect setup at start up, no
 * default values. Which means, when the driver restarted, or system warm
 * reboot, your setup will remained. After cold start, It will load the setup
 * that the BIOS does.
 *
 * We create a sysfs 'name' of the zone, point out where the sensor is. Such
 * as CPU0, SYS3, etc.
 * We create a sysfs 'enable' of the cooling device, too. Which can enable or
 * disable this protect.
 *
 * In EIO-IS200 chip. The smart fan has 3 trips. While the temperature:
 * - Touch Trip0: Shutdown --> Cut off the power.
 * - Touch Trip1: Poweroff --> Send the power button signal.
 * - between Trip2 and Trip1: Throttle --> Intermittently hold the CPU.
 *
 *			  PowerOff    Shutdown
 *			      ^	         ^
 *	      Throttle	      |		 |
 *		 |	      |	         |
 *	+--------+------------+----------+---------
 *	0       trip2	     trip1      trip0  (Temp)
 *
 * Copyright (C) 2023 Advantech Corporation. All rights reserved.
 */

#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/mfd/eiois200.h>

#define CMD_THERM_WRITE		 0x10
#define CMD_THERM_READ		 0x11
#define THERM_NUM		 0x04
#define UNIT_PER_TEMP		 100

#define CTRL_STATE		 0x00
#define CTRL_TYPE		 0x01
#define CTRL_ERROR		 0x04
#define CTRL_VALUE		 0x10
#define CTRL_MAX		 0x11
#define CTRL_MIN		 0x12
#define CTRL_THROTTLE		 0x20
#define CTRL_THROTTLE_HI	 0x21
#define CTRL_THROTTLE_LO	 0x22
#define CTRL_THROTTLE_DEFAULT	 0x28
#define CTRL_THROTTLE_HI_DEFAULT 0x29
#define CTRL_THROTTLE_LO_DEFAULT 0x2A
#define CTRL_POWEROFF		 0x30
#define CTRL_POWEROFF_HI	 0x31
#define CTRL_POWEROFF_LO	 0x32
#define CTRL_POWEROFF_DEFAULT	 0x38
#define CTRL_POWEROFF_HI_DEFAULT 0x30
#define CTRL_POWEROFF_LO_DEFAULT 0x3A
#define CTRL_SHUTDOWN		 0x40
#define CTRL_SHUTDOWN_HI	 0x41
#define CTRL_SHUTDOWN_LO	 0x42
#define CTRL_SHUTDOWN_DEFAULT	 0x48
#define CTRL_SHUTDOWN_HI_DEFAULT 0x49
#define CTRL_SHUTDOWN_LO_DEFAULT 0x4A
#define CTRL_SB_TSI_STATUS	 0x80
#define CTRL_SB_TSI_ACCESS	 0x81
#define CTRL_WARN_STATUS	 0x90
#define CTRL_WARN_BEEP		 0x91
#define CTRL_WARN_TEMP		 0x92

#define THERM_ERR_NO		 0x00
#define THERM_ERR_CHANNEL	 0x01
#define THERM_ERR_HI		 0x02
#define THERM_ERR_LO		 0x03

#define NAME_SIZE		 5

#define TRIP_NUM		 3
#define TRIP_SHUTDOWN		 0
#define TRIP_POWEROFF		 1
#define TRIP_THROTTLE		 2
/* Beep mechanism no stable. Not supported, yet. */
#define TRIP_BEEP		 3

#define DECI_KELVIN_TO_MILLI_CELSIUS(t) (((t) - 2731) * 100)
#define MILLI_CELSIUS_TO_DECI_KELVIN(t) ((t / 100) + 2731)

#define DEV_CH(val)		(((long)(val)) >> 8)
#define DEV_TRIP(val)		(((long)(val)) & 0x0F)
#define TO_DRVDATA(ch, trip)	((((long)(ch)) << 8) | (trip))

#define THERM_WRITE(dev, ctl, id, data) \
	pmc_cmd(dev, CMD_THERM_WRITE, ctl, id, pmc_len[ctl], data)

#define THERM_READ(dev, ctl, id, data) \
	pmc_cmd(dev, CMD_THERM_READ, ctl, id, pmc_len[ctl], data)

#ifndef dev_err_probe
	#define dev_err_probe(dev, ret, fmt, args...) do { \
		dev_err(dev, fmt, ##args); \
		return ret; \
	} while (0)
#endif

union thermal_status {
	struct {
		u8 avail : 1;
		u8 throttle_avail : 1;
		u8 poweroff_avail : 1;
		u8 shutdown_avail : 1;
		u8 throttle_event : 1;
		u8 poweroff_event : 1;
		u8 shutdown_event : 1;
		u8 reserved1 :1;
		u8 throttle_on : 1;
		u8 poweroff_on : 1;
		u8 shutdown_on : 1;
		u8 reserved2 :1;
		u8 throttle_log : 1;
		u8 poweroff_log : 1;
		u8 shutdown_log : 1;
		u8 reserved3 :1;
	};
	u16 value;
};

static u8 pmc_len[] = {
/*      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, a, b, c, d, e, f */
/* 0 */	2, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 1 */	2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 2 */	1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 3 */	1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 4 */	1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 5 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 6 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 7 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 8 */	1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 9 */	2, 1, 2,
};

static char therm_name[0x20][NAME_SIZE + 1] = {
	"CPU0", "CPU1", "CPU2", "CPU3", "SYS0", "SYS1", "SYS2", "SYS3",
	"AUX0", "AUX1", "AUX2", "AUX3", "DIMM0", "DIMM1", "DIMM2", "DIMM3",
	"PCH", "VGA", "", "", "", "", "", "",
	"", "", "", "", "OEM0", "OEM1", "OEM2", "OEM3",
};

static int timeout;
module_param(timeout, int, 0444);
MODULE_PARM_DESC(timeout, "Set PMC command timeout value.\n");

static int pmc_cmd(struct device *dev, u8 cmd,
		   u8 ctrl, u8 id, u8 len, void *data)
{
	struct pmc_op op = {
		.cmd       = cmd,
		.control   = ctrl,
		.device_id = id,
		.size	   = len,
		.payload   = (u8 *)data,
		.timeout   = timeout,
	};

	return eiois200_core_pmc_operation(dev, &op);
}

static ssize_t name_show(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	struct thermal_zone_device *zone =
			container_of(dev, struct thermal_zone_device, device);
	long id = (long)zone->devdata;
	int name = 0;
	int ret;

	ret = THERM_READ(dev, CTRL_TYPE, id, &name);
	if (ret)
		return 0;

	sprintf(buf, "%s\n", therm_name[name]);

	return strlen(buf);
}

static ssize_t enable_store(struct device *dev,
			    struct device_attribute *attr,
			 const char *buf,
			 size_t size)
{
	int ret;
	struct thermal_cooling_device *cdev =
			container_of(dev, struct thermal_cooling_device, device);
	long id = DEV_CH(cdev->devdata);
	long trip = DEV_TRIP(cdev->devdata);
	int enable = 0;
	static u8 ctrl[] = {
		CTRL_SHUTDOWN,
		CTRL_POWEROFF,
		CTRL_THROTTLE
	};

	if (strncasecmp(buf, "enable", strlen("enable")) == 0)
		enable = true;
	else if (strncasecmp(buf, "disable", strlen("disable")) == 0)
		enable = false;
	else
		return -EINVAL;

	ret = THERM_WRITE(dev, ctrl[trip], id, &enable);
	if (ret)
		return ret;

	return size;
}

static ssize_t enable_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	int ret;
	struct thermal_cooling_device *cdev =
			container_of(dev,
				     struct thermal_cooling_device, device);
	long id = DEV_CH(cdev->devdata);
	long trip = DEV_TRIP(cdev->devdata);
	int enable = 0;
	static u8 ctrl[] = {
		CTRL_SHUTDOWN,
		CTRL_POWEROFF,
		CTRL_THROTTLE
	};

	ret = THERM_READ(dev, ctrl[trip], id, &enable);
	if (ret)
		return ret;

	strcpy(buf, (enable & true) == true ? "enabled\n" : "disabled\n");

	return strlen(buf);
}

static DEVICE_ATTR_RO(name);
static DEVICE_ATTR_RW(enable);

static int get_temp(struct thermal_zone_device *zone, int *temp)
{
	struct device *dev = &zone->device;
	int id = (long)zone->devdata;
	u16 val = 0;
	int ret;

	/* Query temp */
	ret = THERM_READ(dev, CTRL_VALUE, id, &val);
	*temp = DECI_KELVIN_TO_MILLI_CELSIUS(val);

	return ret;
}

static int get_trip_type(struct thermal_zone_device *dev, int id,
			 enum thermal_trip_type *type)
{
	*type = id < TRIP_THROTTLE ? THERMAL_TRIP_CRITICAL : THERMAL_TRIP_HOT;

	return id > TRIP_THROTTLE;
}

static int get_trip_temp(struct thermal_zone_device *zone, int trip, int *temp)
{
	long id = (long)zone->devdata;
	int val = 0;
	int ret;
	static u8 ctrl[] = {
		CTRL_SHUTDOWN_HI,
		CTRL_POWEROFF_HI,
		CTRL_THROTTLE_HI
	};

	ret = THERM_READ(&zone->device, ctrl[trip], id, &val);
	*temp = DECI_KELVIN_TO_MILLI_CELSIUS(val);

	return ret;
}

static int set_trip_temp(struct thermal_zone_device *zone, int trip, int temp)
{
	int val, ret;
	long id = (long)zone->devdata;
	static u8 ctrl[] = {
		CTRL_SHUTDOWN_HI,
		CTRL_POWEROFF_HI,
		CTRL_THROTTLE_HI
	};
	static int dec[] = {10, 5, 1};

	if (id >= TRIP_NUM)
		return -EINVAL;

	/* Set trigger temp */
	val = MILLI_CELSIUS_TO_DECI_KELVIN(temp);
	ret = THERM_WRITE(&zone->device, ctrl[trip], id, &val);

	/* Set clear temp */
	val -= dec[id];
	if (!ret)
		ret = THERM_WRITE(&zone->device, ctrl[trip] + 1, id, &val);

	return ret;
}

static int get_max_state(struct thermal_cooling_device *cdev,
			 unsigned long *state)
{
	int ret;
	long id = DEV_CH(cdev->devdata);
	int max = 0;

	ret = THERM_READ(&cdev->device, CTRL_MAX, id, &max);
	*state = DECI_KELVIN_TO_MILLI_CELSIUS(max);

	return ret;
}

static int get_cur_state(struct thermal_cooling_device *cdev,
			 unsigned long *state)
{
	int ret;
	long id = DEV_CH(cdev->devdata);
	int temp = 0;

	ret = THERM_READ(&cdev->device, CTRL_VALUE, id, &temp);
	*state = DECI_KELVIN_TO_MILLI_CELSIUS(temp);

	return ret;
}

static int set_cur_state(struct thermal_cooling_device *cdev,
			 unsigned long state)
{
	return -ENOTSUPP;
}

static void thermal_cooling_device_release(struct device *dev, void *res)
{
	thermal_cooling_device_unregister(*(struct thermal_cooling_device **)res);
}

static struct thermal_cooling_device *
devm_thermal_cooling_device_register(struct device *dev,
				     char *type, void *devdata,
				const struct thermal_cooling_device_ops *ops)
{
	struct thermal_cooling_device **ptr, *tcd;

	ptr = devres_alloc(thermal_cooling_device_release, sizeof(*ptr),
			   GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	tcd = thermal_cooling_device_register(type, devdata, ops);
	if (IS_ERR(tcd)) {
		devres_free(ptr);
		return tcd;
	}

	*ptr = tcd;
	devres_add(dev, ptr);

	return tcd;
}

static void thermal_zone_device_release(struct device *dev, void *res)
{
	thermal_zone_device_unregister(*(struct thermal_zone_device **)res);
}

static struct thermal_zone_device *
devm_thermal_zone_device_register(struct device *dev,
				  const char *type, int trips, int mask, void *devdata,
	struct thermal_zone_device_ops *ops,
	struct thermal_zone_params *tzp,
	int passive_delay, int polling_delay)
{
	struct thermal_zone_device **ptr, *tzd;

	ptr = devres_alloc(thermal_zone_device_release, sizeof(*ptr),
			   GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	tzd = thermal_zone_device_register(type, trips, mask,
					   devdata, ops, tzp,
					   passive_delay, polling_delay);
	if (IS_ERR(tzd)) {
		devres_free(ptr);
		return tzd;
	}

	*ptr = tzd;
	devres_add(dev, ptr);

	return tzd;
}

static struct thermal_zone_device_ops zone_ops = {
	.get_temp = get_temp,
	.get_trip_type = get_trip_type,
	.get_trip_temp = get_trip_temp,
	.set_trip_temp = set_trip_temp,
};

static struct thermal_cooling_device_ops cooling_ops = {
	.get_max_state = get_max_state,
	.get_cur_state = get_cur_state,
	.set_cur_state = set_cur_state,
};

static struct thermal_zone_params zone_params = {
	.governor_name = "user_space",
	.no_hwmon      = true,
};

static int probe(struct platform_device *pdev)
{
	long ch;
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct {
		u8 enable;
		u8 temp;
		u8 hyst;
	} ctrl[] = {
		{ CTRL_SHUTDOWN, CTRL_SHUTDOWN_HI, CTRL_SHUTDOWN_LO },
		{ CTRL_POWEROFF, CTRL_POWEROFF_HI, CTRL_POWEROFF_LO },
		{ CTRL_THROTTLE, CTRL_THROTTLE_HI, CTRL_THROTTLE_LO },
	};

	/* Confirm if eiois200_core exist */
	if (!dev_get_drvdata(dev->parent)) {
		dev_err(dev, "Error contact eiois200_core %d\n", ret);
		return -ENOMEM;
	}

	/* Init and register 4 thermal channel */
	for (ch = 0; ch < THERM_NUM; ch++) {
		union thermal_status state;
		u8 name;
		int trip;
		int hi[] = { 0, 0, 0, 0 };
		struct thermal_zone_device *zone;
		struct thermal_cooling_device *cdev[TRIP_NUM];
		int temps[TRIP_NUM] = { 0, 0, 0 };

		/* Make sure device available */
		if (THERM_READ(dev, CTRL_STATE, ch, &state) ||
		    THERM_READ(dev, CTRL_TYPE, ch, &name)) {
			dev_dbg(dev, "Thermal %ld: pmc function error\n", ch);
			continue;
		}

		if (!state.avail ||
		    (!state.throttle_avail &&
		     !state.poweroff_avail &&
		     !state.shutdown_avail)) {
			dev_dbg(dev, "Thermal:%ld firmware reports not activated\n", ch);
			continue;
		}

		if (therm_name[name][0] == '\0') {
			dev_dbg(dev, "Unknown thermal sensor name\n");
			continue;
		}

		/* Get all trip value */
		for (trip = 0 ; trip < TRIP_NUM ; trip++) {
			if (THERM_READ(dev, ctrl[trip].temp, ch, &hi[trip]))
				dev_err_probe(dev, -EIO, "Read thermal_%ld error\n",
					      ch);

			temps[trip] = DECI_KELVIN_TO_MILLI_CELSIUS(hi[trip]);
		}

		/* Create zone */
		zone = devm_thermal_zone_device_register(
				dev, "eiois200_thermal", TRIP_NUM,
				(1 << TRIP_NUM) - 1, (void *)ch,
				&zone_ops, &zone_params, 0, 0);
		if (!zone)
			return PTR_ERR(zone);

		ret = device_create_file(&zone->device, &dev_attr_name);
		if (ret)
			dev_warn(dev, "Error create thermal zone name sysfs\n");

		/* Create 3 cooling device and bind them */
		for (trip = 0; trip < TRIP_NUM; trip++) {
			int lo = temps[trip];
			int hi = lo;

			if (trip) {
				hi = temps[trip - 1];
				lo = hi < lo ? hi : lo; /* swap on invalid */
			}

			cdev[trip] = devm_thermal_cooling_device_register(
						dev, "Processor",
						(void *)TO_DRVDATA(ch, trip),
						&cooling_ops);
			if (IS_ERR(cdev[trip]))
				dev_err_probe(dev, PTR_ERR(cdev[trip]),
					      "Create thermal cooling device failed:%ld\n",
					      PTR_ERR(cdev[trip]));

			ret = thermal_zone_bind_cooling_device(
					zone, trip, cdev[trip],
					hi, lo, THERMAL_WEIGHT_DEFAULT);
			if (ret)
				dev_err_probe(dev, ret, "Error binding cooling device\n");

			ret = device_create_file(&cdev[trip]->device, &dev_attr_enable);
			if (ret)
				dev_warn(dev, "Error create cooling device enable sysfs\n");
		}

#if defined(thermal_zone_device_enable)
		thermal_zone_device_enable(zone);
#endif
		dev_dbg(dev, "%s thermal protect up\n", therm_name[name]);
	}

	return 0;
}

static struct platform_driver tz_driver = {
	.driver = {
		.name = "eiois200_thermal",
	},
};

module_platform_driver_probe(tz_driver, probe);

MODULE_AUTHOR("Adavantech");
MODULE_DESCRIPTION("GPIO driver for Advantech EIO-IS200 embedded controller");
MODULE_LICENSE("GPL v2");
