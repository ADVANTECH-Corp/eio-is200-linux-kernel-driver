// SPDX-License-Identifier: GPL-2.0-only
/*
 * eiois200_fan
 * ============
 * Thermal zone driver for Advantech EIO-IS200 embedded controller's smart
 * fan mechanism. This driver currently support EIO-IS200 smart-fan hardware
 * auto mode only. Not support other governors by software.
 *
 * The driver will load the EC smart fan current setup at start up, not use
 * the default value. Which means, when the driver restarted, or system warm
 * reboot, all your setup will remained. After cold start, It will load the
 * setup that the BIOS does.
 *
 * We create a sysfs 'name' of the zone, point out where the fan is. Such as
 * CPU0, SYS3, etc.
 * We create a sysfs 'set_max_state' of the cooling device, too. Which can
 * modifies the threshold of the fan PWM value of a thermal zone trip.
 *
 * In EIO-IS200 chip. The smart fan has 3 trips. When the temperature is:
 * - Over Temp High(trip0), the Fan runs at the fan PWM High.
 * - Between Temp Low and Temp High(trip1 - trip0), the fan PWM value slopes
 *   from PWM Low to PWM High.
 * - Between Temp Stop and Temp Low(trip2 - trip1), the fan PWM is PWM low.
 * - Below Temp Stop, the fan stopped.
 *
 * (PWM)|
 *  	|
 * High |............................. ______________
 *  	|			      /:
 *  	|			     / :
 *  	|			    /  :
 *  	|			   /   :
 *  	|			  /    :
 *  	|			 /     :
 *  	|			/      :  
 *  	|		       /       :
 *  Low	|.......... __________/	       :
 *  	|	    |	      :	       :
 *  	|	    |	      :	       :
 *    0	+===========+---------+--------+-------------
 *  	0 	   Stop	     Low      High	(Temp)
 *
 * Copyright (C) 2023 Advantech Corporation. All rights reserved.
 */

#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/mfd/core.h>
#include <linux/thermal.h>
#include <linux/mfd/eiois200.h>

#define CMD_FAN_WRITE		0x24
#define CMD_FAN_READ		0x25
#define FAN_MAX			0x04

#define CMD_THERM_WRITE		0x10
#define CMD_THERM_READ		0x11
#define THERM_MAX		0x04
#define THERM_MULTI		100

#define CTRL_STATE		0x00
#define CTRL_TYPE		0x01
#define CTRL_CTRL		0x02
#define CTRL_ERROR		0x04
#define CTRL_VALUE		0x10
#define CTRL_INVERT		0x11
#define CTRL_FREQ		0x12
#define CTRL_THERM_HIGH		0x13
#define CTRL_THERM_LOW		0x14
#define CTRL_THERM_STOP		0x15
#define CTRL_PWM_HIGH		0x16
#define CTRL_PWM_LOW		0x17
#define CTRL_THERM_SRC		0x20

#define CTRLMODE_STOP		0x00
#define CTRLMODE_FULL		0x01
#define CTRLMODE_MANUAL		0x02
#define CTRLMODE_AUTO		0x03

#define DUTY_MAX		100
#define UNIT_PER_TEMP		10
#define NAME_SIZE		4

#define TRIP_HIGH		0
#define TRIP_LOW		1
#define TRIP_STOP		2
#define TRIP_NUM		3

#define FAN_SRC(val)		((long)(val) >> 4)
#define FAN_ID(val)		(((long)(val)) >> 8)
#define FAN_TRIP(val)		(((long)(val)) & 0x0F)

#define DECI_KELVIN_TO_MILLICELSIUS(t)	(((t) - 2731) * 100)
#define MILLICELSIUS_TO_DECI_KELVIN(t)	((t) / 100 + 2731)

#define FAN_WRITE(dev, ctl, id, data) \
	pmc_cmd(dev, CMD_FAN_WRITE, ctl, id, pmc_len[ctl], data)

#define FAN_READ(dev, ctl, id, data) \
	pmc_cmd(dev, CMD_FAN_READ, ctl, id, pmc_len[ctl], data)

static u8 pmc_len[CTRL_THERM_SRC + 1] = {
/*      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, a, b, c, d, e, f */
	1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 4, 2, 2, 2, 1, 1, 2, 2, 2, 0, 0, 0, 0, 0,
	1,
};

static char fan_name[0x20][NAME_SIZE + 1] = {
	"CPU0", "CPU1", "CPU2", "CPU3", "SYS0", "SYS1", "SYS2", "SYS3",
	"", "", "", "", "", "", "", "",
	"", "", "", "", "", "", "", "",
	"", "", "", "", "OEM0", "OEM1", "OEM2", "OEM3",
};

static int pmc_cmd(struct device *dev, u8 cmd, u8 ctrl, u8 id, u8 len, void *data)
{
	struct pmc_op op = {
		.cmd       = cmd,
		.control   = ctrl,
		.device_id = id,
		.size	   = len,
		.payload   = (u8 *)data,
	};

	return eiois200_core_pmc_operation(dev, &op);
}

static ssize_t set_max_state_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct thermal_cooling_device *cdev =
			container_of(dev, struct thermal_cooling_device, device);

	long id = FAN_ID(cdev->devdata);
	long trip = FAN_TRIP(cdev->devdata);
	long max = 0;
	int ret = 0;

	if (kstrtol(buf, 10, &max))
		return count;

	if (trip <= TRIP_LOW)
		ret = FAN_WRITE(dev, CTRL_PWM_HIGH + trip, id, &max);
	else
		dev_warn(dev, "This device doesn't support write max state\n");

	if (ret)
		dev_err(dev, "Write cooling device max state error: %d\n", ret);

	thermal_cooling_device_update(cdev);

	return count;
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

	ret = FAN_READ(dev, CTRL_TYPE, id, &name);
	if (ret)
		return 0;

	sprintf(buf, "%s\n", fan_name[name]);

	return strlen(buf);
}

static DEVICE_ATTR_WO(set_max_state);
static DEVICE_ATTR_RO(name);

static int get_temp(struct thermal_zone_device *zone, int *temp)
{
	struct device *dev = &zone->device;
	int id = (long)zone->devdata;
	int sensor = 0;
	u16 val = 0;
	int ret;

	/* Query which sensor */
	ret = FAN_READ(dev, CTRL_CTRL, id, &sensor);
	if (ret)
		return ret;

	/* Query temp */
	ret = pmc_cmd(dev, CMD_THERM_READ, CTRL_VALUE,
		      FAN_SRC(sensor), sizeof(val), &val);

	*temp = DECI_KELVIN_TO_MILLICELSIUS(val);

	return ret;
}

static int change_mode(struct thermal_zone_device *zone,
		       enum thermal_device_mode mode)
{
	return 0;
}

static int get_trip_type(struct thermal_zone_device *dev, int id,
			 enum thermal_trip_type *type)
{
	*type = THERMAL_TRIP_ACTIVE;

	return 0;
}

static int get_trip_temp(struct thermal_zone_device *zone, int trip, int *temp)
{
	long id = (long)zone->devdata;
	int val = 0;
	int ret;

	ret = FAN_READ(&zone->device, CTRL_THERM_HIGH + trip, id, &val);

	*temp = DECI_KELVIN_TO_MILLICELSIUS(val);

	return ret;
}

static int set_trip_temp(struct thermal_zone_device *zone, int trip, int temp)
{
	long id = (long)zone->devdata;
	int val, ret;
	
	if (temp < 1000)
		return -EINVAL;

	val = MILLICELSIUS_TO_DECI_KELVIN(temp);
	
	ret = FAN_WRITE(&zone->device, CTRL_THERM_HIGH + trip, id, &val);

	return ret;
}

static int get_max_state(struct thermal_cooling_device *cdev,
			 unsigned long *state)
{
	long id = FAN_ID(cdev->devdata);
	long trip = FAN_TRIP(cdev->devdata);

	if (trip <= TRIP_LOW)
		return FAN_READ(&cdev->device, CTRL_PWM_HIGH + trip, id, state);

	return 0;
}

static int get_cur_state(struct thermal_cooling_device *cdev,
			 unsigned long *state)
{
	long id = FAN_ID(cdev->devdata);

	*state = 0;

	return FAN_READ(&cdev->device, CTRL_VALUE, id, state);
}

static int set_cur_state(struct thermal_cooling_device *cdev,
			 unsigned long state)
{
	return -ENOSYS;
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

static struct thermal_zone_device
*devm_thermal_zone_device_register_with_trips(struct device *dev,
	const char *type, struct thermal_trip *trips,
	int num_trips, int mask, void *devdata,
	struct thermal_zone_device_ops *ops,
	struct thermal_zone_params *tzp, int passive_delay,
	int polling_delay)
{
	struct thermal_zone_device **ptr, *tzd;

	ptr = devres_alloc(thermal_zone_device_release, sizeof(*ptr),
			   GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	tzd = thermal_zone_device_register_with_trips(type, trips, num_trips,
						      mask, devdata, ops, tzp,
						      passive_delay,
						      polling_delay);
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
	.change_mode = change_mode,
	.get_trip_type = get_trip_type,
	.get_trip_temp = get_trip_temp,
	.set_trip_temp = set_trip_temp,
};

static struct thermal_cooling_device_ops cooling_ops = {
	.get_max_state = get_max_state,
	.get_cur_state = get_cur_state,
	.set_cur_state = set_cur_state,
};

static struct thermal_trip trips[4][3] = {{{ 0, 0, THERMAL_TRIP_ACTIVE }}};

static struct thermal_zone_params zone_params = {
	.governor_name = "user_space",
	.no_hwmon      = true,
};

static int probe(struct platform_device *pdev)
{
	long fan;
	int ret = 0;
	struct device *dev =  &pdev->dev;

	/* Confirm if eiois200_core exist */
	if (!dev_get_drvdata(dev->parent))
		return dev_err_probe(dev, -ENOMEM,
				     "Error contact eiois200_core %d\n", ret);

	/* Init and register 4 smart fan */
	for (fan = 0; fan < FAN_MAX; fan++) {
		u8 state, name;
		int trip;
		int trip_hi = 0, trip_lo = 0, trip_stop = 0;
		int pwm_hi = 0, pwm_lo = 0;
		struct thermal_zone_device *zone;
		struct thermal_cooling_device *cdev[3];

		/* Read the fan's all params */
		if (FAN_READ(dev, CTRL_STATE,	   fan, &state)	    ||
		    FAN_READ(dev, CTRL_TYPE,	   fan, &name)	    ||
		    FAN_READ(dev, CTRL_THERM_HIGH, fan, &trip_hi)   ||
		    FAN_READ(dev, CTRL_THERM_LOW,  fan, &trip_lo)   ||
		    FAN_READ(dev, CTRL_THERM_STOP, fan, &trip_stop) ||
		    FAN_READ(dev, CTRL_PWM_HIGH,   fan, &pwm_hi)    ||
		    FAN_READ(dev, CTRL_PWM_LOW,    fan, &pwm_lo)) {
			dev_dbg(dev, "Smart fan%ld: pmc function error\n", fan);
			continue;
		}

		if ((state & 1) == 0) {
			dev_dbg(dev, "Smart fan:%ld firmware reports not activated\n", fan);
			continue;
		}

		if (fan_name[name][0] == '\0') {
			dev_dbg(dev, "Unknown fan name\n");
			continue;
		}

		trips[fan][0].temperature = DECI_KELVIN_TO_MILLICELSIUS(trip_hi);
		trips[fan][1].temperature = DECI_KELVIN_TO_MILLICELSIUS(trip_lo);
		trips[fan][2].temperature = DECI_KELVIN_TO_MILLICELSIUS(trip_stop);

		/* Create zone */
		zone = devm_thermal_zone_device_register_with_trips(dev,
				"eiois200_fan", trips[fan], ARRAY_SIZE(trips[fan]),
				(1 << ARRAY_SIZE(trips[fan])) - 1,
				(void *)fan, &zone_ops, &zone_params, 0, 0);
		if (!zone)
			return PTR_ERR(zone);

		/* The same fan but different range */
		for (trip = 0; trip < TRIP_NUM; trip++) {
			int hi[] = { pwm_hi, pwm_lo, 0 };
			int lo[] = { pwm_lo, pwm_lo, 0 } ;
				 
			cdev[trip] = devm_thermal_cooling_device_register(dev,
						"Fan",
						(void *)((fan << 8) | trip),
						&cooling_ops);
			if (IS_ERR(cdev[trip]))
				return dev_err_probe(dev, PTR_ERR(cdev[trip]),
						     "Create smart fan cooling device failed:%ld\n",
						     PTR_ERR(cdev[trip]));

			ret = thermal_zone_bind_cooling_device(zone,
							trip, cdev[trip],
							hi[trip], lo[trip],
							THERMAL_WEIGHT_DEFAULT);
			if (ret)
				return dev_err_probe(dev, ret,
						     "Create binding cooling device failed\n");

			if (trip == TRIP_STOP) 
				continue ;

			ret = device_create_file(&cdev[trip]->device,
						 &dev_attr_set_max_state);
			if (ret)
				dev_warn(dev,
					 "Create set_max_state sysfs failed:%d\n",
					 ret);
		}

		ret = device_create_file(&zone->device, &dev_attr_name);
		if (ret)
			dev_warn(dev, "Error create thermal zone name sysfs\n");

		dev_dbg(dev, "%s smart fan up\n", fan_name[name]);
	}
	
	return ret;
}

static struct platform_driver tz_driver = {
	.driver = {
		.name = "eiois200_fan",
	},
};

module_platform_driver_probe(tz_driver, probe);

MODULE_AUTHOR("Adavantech");
MODULE_DESCRIPTION("GPIO driver for Advantech EIO-IS200 embedded controller");
MODULE_LICENSE("GPL v2");
