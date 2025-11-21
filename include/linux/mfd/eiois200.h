/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Header for the Advantech EIO-IS200 core driver and its sub-drivers
 *
 * Copyright (C) 2023 Advantech Co., Ltd.
 * Author: wenkai.chung <wenkai.chung@advantech.com.tw>
 */

#ifndef _MFD_EIOIS200_H_
#define _MFD_EIOIS200_H_
#include <linux/io.h>
#include <linux/regmap.h>
#include <linux/thermal.h>
#include <uapi/linux/thermal.h>
#include <linux/version.h>

#define THERMAL_NO_TARGET -1UL

#if KERNEL_VERSION(6, 14, 0) <= LINUX_VERSION_CODE

struct thermal_attr {
	struct device_attribute attr;
	char name[THERMAL_NAME_LENGTH];
};

struct thermal_trip_attrs {
	struct thermal_attr type;
	struct thermal_attr temp;
	struct thermal_attr hyst;
};

struct thermal_trip_desc {
	struct thermal_trip trip;
	struct thermal_trip_attrs trip_attrs;
	struct list_head list_node;
	struct list_head thermal_instances;
	int threshold;
};

struct thermal_zone_device {
	int id;
	char type[THERMAL_NAME_LENGTH];
	struct device device;
	struct completion removal;
	struct completion resume;
	struct attribute_group trips_attribute_group;
	struct list_head trips_high;
	struct list_head trips_reached;
	struct list_head trips_invalid;
	enum thermal_device_mode mode;
	void *devdata;
	int num_trips;
	unsigned long passive_delay_jiffies;
	unsigned long polling_delay_jiffies;
	unsigned long recheck_delay_jiffies;
	int temperature;
	int last_temperature;
	int emul_temperature;
	int passive;
	int prev_low_trip;
	int prev_high_trip;
	struct thermal_zone_device_ops ops;
	struct thermal_zone_params *tzp;
	struct thermal_governor *governor;
	void *governor_data;
	struct ida ida;
	struct mutex lock;
	struct list_head node;
	struct delayed_work poll_queue;
	enum thermal_notify_event notify_event;
	u8 state;
#ifdef CONFIG_THERMAL_DEBUGFS
	struct thermal_debugfs *debugfs;
#endif
	struct list_head user_thresholds;
	struct thermal_trip_desc trips[] __counted_by(num_trips);
};

struct thermal_instance {
	int id;
	char name[THERMAL_NAME_LENGTH];
	struct thermal_cooling_device *cdev;
	const struct thermal_trip *trip;
	bool initialized;
	unsigned long upper;	/* Highest cooling state for this trip point */
	unsigned long lower;	/* Lowest cooling state for this trip point */
	unsigned long target;	/* expected cooling state */
	char attr_name[THERMAL_NAME_LENGTH];
	struct device_attribute attr;
	char weight_attr_name[THERMAL_NAME_LENGTH];
	struct device_attribute weight_attr;
	struct list_head trip_node; /* node in trip->thermal_instances */
	struct list_head cdev_node; /* node in cdev->thermal_instances */
	unsigned int weight; /* The weight of the cooling device */
	bool upper_no_limit;
};

struct thermal_governor {
	const char *name;
	int (*bind_to_tz)(struct thermal_zone_device *tz);
	void (*unbind_from_tz)(struct thermal_zone_device *tz);
	void (*trip_crossed)(struct thermal_zone_device *tz,
				const struct thermal_trip *trip,
				bool upward);
	void (*manage)(struct thermal_zone_device *tz);
	void (*update_tz)(struct thermal_zone_device *tz,
				enum thermal_notify_event reason);
	struct list_head        governor_list;
};

#define trip_to_trip_desc(__trip)       \
	container_of(__trip, struct thermal_trip_desc, trip)

#define to_thermal_zone(_dev) \
	container_of(_dev, struct thermal_zone_device, device)

void thermal_governor_update_tz(struct thermal_zone_device *tz,
				enum thermal_notify_event reason)
{
	if (!tz->governor || !tz->governor->update_tz)
		return;

	tz->governor->update_tz(tz, reason);
}

ssize_t
weight_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct thermal_instance *instance;

	instance = container_of(attr, struct thermal_instance, weight_attr);

	return sprintf(buf, "%d\n", instance->weight);
}

DEFINE_GUARD(thermal_zone, struct thermal_zone_device *, mutex_lock(&_T->lock), mutex_unlock(&_T->lock))

ssize_t weight_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct thermal_zone_device *tz = to_thermal_zone(dev);
	struct thermal_instance *instance;
	int ret, weight;

	ret = kstrtoint(buf, 0, &weight);
	if (ret)
		return ret;

	instance = container_of(attr, struct thermal_instance, weight_attr);

	/* Don't race with governors using the 'weight' value */
	guard(thermal_zone)(tz);

	instance->weight = weight;
	thermal_governor_update_tz(tz, THERMAL_INSTANCE_WEIGHT_CHANGED);

	return count;
}


int thermal_zone_trip_id(const struct thermal_zone_device *tz,
			const struct thermal_trip *trip)
{
	/*
	 * Assume the trip to be located within the bounds of the thermal
	 * zone's trips[] table.
	*/
	return trip_to_trip_desc(trip) - tz->trips;
}

ssize_t
trip_point_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct thermal_zone_device *tz = to_thermal_zone(dev);
	struct thermal_instance *instance;

	instance = container_of(attr, struct thermal_instance, attr);

	return sprintf(buf, "%d\n", thermal_zone_trip_id(tz, instance->trip));
}

static int thermal_instance_add(struct thermal_instance *new_instance,
				struct thermal_cooling_device *cdev,
				struct thermal_trip_desc *td)
{
	struct thermal_instance *instance;

	list_for_each_entry(instance, &td->thermal_instances, trip_node) {
		if (instance->cdev == cdev)
			return -EEXIST;
	}

	list_add_tail(&new_instance->trip_node, &td->thermal_instances);
	guard(cooling_dev)(cdev);
	list_add_tail(&new_instance->cdev_node, &cdev->thermal_instances);

	return 0;
}

static int thermal_bind_cdev_to_trip(struct thermal_zone_device *tz,
					struct thermal_trip_desc *td,
					struct thermal_cooling_device *cdev,
					struct cooling_spec *cool_spec)
{
	struct thermal_instance *dev;
	bool upper_no_limit;
	int result;

	/* lower default 0, upper default max_state */
	if (cool_spec->lower == THERMAL_NO_LIMIT)
		cool_spec->lower = 0;

	if (cool_spec->upper == THERMAL_NO_LIMIT) {
		cool_spec->upper = cdev->max_state;
		upper_no_limit = true;
	} else {
		upper_no_limit = false;
	}

	if (cool_spec->lower > cool_spec->upper || cool_spec->upper > cdev->max_state)
		return -EINVAL;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->cdev = cdev;
	dev->trip = &td->trip;
	dev->upper = cool_spec->upper;
	dev->upper_no_limit = upper_no_limit;
	dev->lower = cool_spec->lower;
	dev->target = THERMAL_NO_TARGET;
	dev->weight = cool_spec->weight;

	result = ida_alloc(&tz->ida, GFP_KERNEL);
	if (result < 0)
		goto free_mem;

	dev->id = result;
	sprintf(dev->name, "cdev%d", dev->id);
	result =
	    sysfs_create_link(&tz->device.kobj, &cdev->device.kobj, dev->name);
	if (result)
		goto release_ida;

	snprintf(dev->attr_name, sizeof(dev->attr_name), "cdev%d_trip_point",
		 dev->id);
	sysfs_attr_init(&dev->attr.attr);
	dev->attr.attr.name = dev->attr_name;
	dev->attr.attr.mode = 0444;
	dev->attr.show = trip_point_show;
	result = device_create_file(&tz->device, &dev->attr);
	if (result)
		goto remove_symbol_link;

	snprintf(dev->weight_attr_name, sizeof(dev->weight_attr_name),
		 "cdev%d_weight", dev->id);
	sysfs_attr_init(&dev->weight_attr.attr);
	dev->weight_attr.attr.name = dev->weight_attr_name;
	dev->weight_attr.attr.mode = S_IWUSR | S_IRUGO;
	dev->weight_attr.show = weight_show;
	dev->weight_attr.store = weight_store;
	result = device_create_file(&tz->device, &dev->weight_attr);
	if (result)
		goto remove_trip_file;

	result = thermal_instance_add(dev, cdev, td);
	if (result)
		goto remove_weight_file;

	thermal_governor_update_tz(tz, THERMAL_TZ_BIND_CDEV);

	return 0;

remove_weight_file:
	device_remove_file(&tz->device, &dev->weight_attr);
remove_trip_file:
	device_remove_file(&tz->device, &dev->attr);
remove_symbol_link:
	sysfs_remove_link(&tz->device.kobj, dev->name);
release_ida:
	ida_free(&tz->ida, dev->id);
free_mem:
	kfree(dev);
	return result;
}

#endif

/* Definition */
#define EIOIS200_CHIPID1		0x20
#define EIOIS200_CHIPID2		0x21
#define EIOIS200_CHIPVER		0x22
#define EIOIS200_SIOCTRL		0x23
#define EIOIS200_SIOCTRL_SIOEN		BIT(0)
#define EIOIS200_SIOCTRL_SWRST		BIT(1)
#define EIOIS200_IRQCTRL		0x70
#define EIOIS200_CHIPID			0x9610
#define EIO201_211_CHIPID		0x9620
#define EIOIS200_ICCODE			0x10
#define EIO201_ICCODE			0x20
#define EIO211_ICCODE			0x21

/* LPC PNP */
#define EIOIS200_PNP_INDEX		0x299
#define EIOIS200_PNP_DATA		0x29A
#define EIOIS200_SUB_PNP_INDEX		0x499
#define EIOIS200_SUB_PNP_DATA		0x49A
#define EIOIS200_EXT_MODE_ENTER		0x87
#define EIOIS200_EXT_MODE_EXIT		0xAA

/* LPC LDN */
#define EIOIS200_LDN			0x07
#define EIOIS200_LDN_PMC0		0x0C
#define EIOIS200_LDN_PMC1		0x0D

/* PMC registers */
#define EIOIS200_PMC_STATUS_IBF		BIT(1)
#define EIOIS200_PMC_STATUS_OBF		BIT(0)
#define EIOIS200_LDAR			0x30
#define EIOIS200_LDAR_LDACT		BIT(0)
#define EIOIS200_IOBA0H			0x60
#define EIOIS200_IOBA0L			0x61
#define EIOIS200_IOBA1H			0x62
#define EIOIS200_IOBA1L			0x63
#define EIOIS200_FLAG_PMC_READ		BIT(0)

/* PMC command list */
#define EIOIS200_PMC_CMD_ACPIRAM_READ	0x31
#define EIOIS200_PMC_CMD_CFG_SAVE	0x56

/* OLD PMC */
#define EIOIS200_PMC_NO_INDEX		0xFF

/* ACPI RAM Address Table */
#define EIOIS200_ACPIRAM_VERSIONSECTION	(0xFA)
#define EIOIS200_ACPIRAM_ICVENDOR	(EIOIS200_ACPIRAM_VERSIONSECTION + 0x00)
#define EIOIS200_ACPIRAM_ICCODE		(EIOIS200_ACPIRAM_VERSIONSECTION + 0x01)
#define EIOIS200_ACPIRAM_CODEBASE	(EIOIS200_ACPIRAM_VERSIONSECTION + 0x02)

#define EIOIS200_ACPIRAM_CODEBASE_NEW	BIT(7)

/* Firmware */
#define EIOIS200_F_SUB_NEW_CODE_BASE	BIT(6)
#define EIOIS200_F_SUB_CHANGED		BIT(7)
#define EIOIS200_F_NEW_CODE_BASE	BIT(8)
#define EIOIS200_F_CHANGED		BIT(9)
#define EIOIS200_F_SUB_CHIP_EXIST	BIT(30)
#define EIOIS200_F_CHIP_EXIST		BIT(31)

/* Others */
#define EIOIS200_EC_NUM	2

struct _pmc_port {
	union {
		u16 cmd;
		u16 status;
	};
	u16 data;
};

struct pmc_op {
	u8  cmd;
	u8  control;
	u8  device_id;
	u8  size;
	u8  *payload;
	u8  chip;
	u16 timeout;
};

enum eiois200_rw_operation {
	OPERATION_READ,
	OPERATION_WRITE,
};

struct eiois200_dev {
	u32 flag;

	struct _pmc_port  pmc[EIOIS200_EC_NUM];

	struct mutex mutex; /* Protects PMC command access */
};

/**
 * eiois200_core_pmc_operation - Execute a new pmc command
 * @dev:	The device structure pointer.
 * @op:		Pointer to an new pmc command.
 */
int eiois200_core_pmc_operation(struct device *dev,
				struct pmc_op *operation);

enum eiois200_pmc_wait {
	PMC_WAIT_INPUT,
	PMC_WAIT_OUTPUT,
};

/**
 * eiois200_core_pmc_wait - Wait for input / output buffer to be ready
 * @dev:	The device structure pointer.
 * @id:		0 for main chip, 1 for sub chip.
 * @wait:	%PMC_WAIT_INPUT or %PMC_WAIT_OUTPUT.
 *		%PMC_WAIT_INPUT for waiting input buffer data ready.
 *		%PMC_WAIT_OUTPUT for waiting output buffer empty.
 * @timeout:	The timeout value. 0 means use the default value.
 */
int eiois200_core_pmc_wait(struct device *dev,
			   int id,
			   enum eiois200_pmc_wait wait,
			   uint timeout);

#define WAIT_IBF(dev, id, timeout)	eiois200_core_pmc_wait(dev, id, PMC_WAIT_INPUT, timeout)
#define WAIT_OBF(dev, id, timeout)	eiois200_core_pmc_wait(dev, id, PMC_WAIT_OUTPUT, timeout)

#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#endif

#endif
