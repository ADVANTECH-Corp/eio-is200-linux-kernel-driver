// SPDX-License-Identifier: GPL-2.0-only
/*
 * GPIO driver for Advantech EIO-IS200 embedded controller.
 *
 * Copyright (C) 2023 Advantech Corporation. All rights reserved.
 */

#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/mfd/core.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/mfd/eiois200.h>

#define MAX_DEV 128
#define MAX_NAME 32

static uint timeout = 2000;
module_param(timeout, uint, 0664);
MODULE_PARM_DESC(timeout,
		 "Default pmc command timeout in micro-seconds.\n");

static struct eiois200_dev *eiois200_dev;

static struct _hwmon_dev {
	struct device *dev;
	struct regmap *regmap;
} *hwmon_dev = NULL;

enum _sen_type {
	NONE,
	VOLTAGE,
	CURRENT,
	TEMP,
	PWM,
	TACHO,
	FAN,
	CASEOPEN,
};

static struct {
	u8   cmd;
	u8   max;
	int  shift;
	char name[32];
	u8   ctrl[16];
	u16  multi[16];
	char item[16][32];
	char labels[32][32];

} sen_info[] = {
	{ 0x00, 0, 0, "none" },
	{ 0x12, 8, 0, "in",
		{ 0xFF, 0x10, 0x11, 0x12 },
		{ 1,    10,   10,   10 },
		{ "label", "input", "max", "min" },
		{ "5V", "5Vs5", "12V", "12Vs5",
		  "3V3", "3V3", "5Vsb", "3Vsb",
		  "Vcmos", "Vbat", "Vdc", "Vstb",
		  "Vcore_a", "Vcore_b", "", "",
		  "Voem0", "Voem1", "Voem2", "Voem3"
		},
	},
	{ 0x1a, 2, 0, "curr",
		{ 0xFF, 0x10, 0x11, 0x12 },
		{ 1,    10,   10,   10 },
		{ "label", "input", "max", "min" },
		{ "dc", "oem0" },
	},
	{ 0x10, 4, -2731, "temp",
		{ 0xFF, 0x10, 0x11, 0x12, 0x21, 0x41 },
		{ 1,    100,  100,  100,  100,  100 },
		{ "label", "input", "max", "min", "crit", "emergency" },
		{ "cpu0", "cpu1", "cpu2", "cpu3",
		  "sys0", "sys1", "sys2", "sys3",
		  "aux0", "aux1", "aux2", "aux3",
		  "dimm0", "dimm1", "dimm2", "dimm3",
		  "pch", "gpu", "", "",
		  "", "", "", "",
		  "", "", "", "",
		  "oem0", "oem1", "oem", "oem3" },
	},
	{ 0x14, 0, 0, "pwm",
		{ 0xFF, 0x11, 0x12 },
		{ 1, 1, 1 },
		{ "label", "polarity", "freq" },
		{ "pwm0", "pwm0", "pwm0", "pwm0" },
	},
	{ 0x16, 2, 0, "tacho",
		{ 0xFF, 0x10 },
		{ 1, 1 },
		{ "label", "input"},
		{ "cpu0", "cpu1", "cpu2", "cpu3",
		  "sys0", "sys1", "sys2", "sys3",
		  "", "", "", "", "", "", "", "",
		  "", "", "", "", "", "", "", "",
		  "", "", "", "",
		  "oem0", "oem1", "oem2", "oem3"
		},
	},
	{ 0x24, 4, 0, "fan",
		{ 0xFF, 0x1A },
		{ 1, 1 },
		{ "label", "input"},
		{ "cpu0", "cpu1", "cpu2", "cpu3",
		  "sys0", "sys1", "sys2", "sys3",
		  "", "", "", "", "", "", "", "",
		  "", "", "", "", "", "", "", "",
		  "", "", "", "",
		  "oem0", "oem1", "oem2", "oem3",
		},
	},
	{ 0x28, 1, 0, "intrusion",
		{ 0xFF, 0x02 },
		{ 1, 1 },
		{ "label", "input" },
		{ "case_open" }
	}
};

static struct {
	enum _sen_type type;
	u8   ctrl;
	int  size;
	bool write;

} ctrl_para[] = {
	{ NONE,     0x00, 0, false },

	{ VOLTAGE,  0x00, 1, false }, { VOLTAGE,  0x01, 1, false },
	{ VOLTAGE,  0x10, 2, false }, { VOLTAGE,  0x11, 2, false },
	{ VOLTAGE,  0x12, 2, false },

	{ CURRENT,  0x00, 1, false }, { CURRENT,  0x01, 1, false },
	{ CURRENT,  0x10, 2, false }, { CURRENT,  0x11, 2, false },
	{ CURRENT,  0x12, 2, false },

	{ TEMP,	    0x00, 2, false }, { TEMP,	  0x01, 1, false },
	{ TEMP,     0x04, 1, false }, { TEMP,	  0x10, 2, false },
	{ TEMP,     0x11, 2, false }, { TEMP,	  0x12, 2, false },
	{ TEMP,     0x21, 2, false }, { TEMP,	  0x41, 2, false },

	{ PWM,      0x00, 1, false }, { PWM,	  0x10, 1, true  },
	{ PWM,      0x11, 1, true  }, { PWM,	  0x12, 4, true  },

	{ TACHO,    0x00, 1, false }, { TACHO,	  0x01, 1, false },
	{ TACHO,    0x10, 4, true  },

	{ FAN,      0x00, 1, false }, { FAN,	  0x01, 1, false },
	{ FAN,      0x03, 1, true  }, { FAN,	  0x1A, 2, false },

	{ CASEOPEN, 0x00, 1, false }, { CASEOPEN, 0x02, 1, true  },
};

static int para_idx(enum _sen_type type, u8 ctrl)
{
	int i;

	for (i = 1 ; i < ARRAY_SIZE(ctrl_para) ; i++)
		if (type == ctrl_para[i].type &&
		    ctrl == ctrl_para[i].ctrl)
			return i;

	return 0;
}

static int pmc_write(enum _sen_type type, u8 dev_id, u8 ctrl, void *data)
{
	int idx = para_idx(type, ctrl);
	struct   pmc_op op = {
		 .cmd       = sen_info[type].cmd,
		 .control   = ctrl,
		 .device_id = dev_id,
		 .size	    = ctrl_para[idx].size,
		 .payload   = (u8 *)data,
		 .timeout   = timeout,
	};

	if (idx == 0)
		return -EINVAL;

	if (!ctrl_para[idx].write)
		return -EINVAL;

	return eiois200_core_pmc_operation(NULL, &op);
}

static int pmc_read(enum _sen_type type, u8 dev_id, u8 ctrl, void *data)
{
	int idx = para_idx(type, ctrl);
	struct   pmc_op op = {
		 .cmd       = sen_info[type].cmd + 1,
		 .control   = ctrl,
		 .device_id = dev_id,
		 .size	    = ctrl_para[idx].size,
		 .payload   = (u8 *)data,
		 .timeout   = timeout,
	};

	if (idx == 0)
		return -EINVAL;

	return eiois200_core_pmc_operation(NULL, &op);
}

static ssize_t show(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	int ret;
	int idx = to_sensor_dev_attr(attr)->index;
	enum _sen_type type = (enum _sen_type)idx >> 24;
	int shift = (idx >> 16) & 0xFF;
	int item = (idx >> 8) & 0xFF;
	int id = idx  & 0xFF;
	u32 data = 0;

	switch (item) {
	case 0:
		return sprintf(buf, "%s\n", sen_info[type].labels[id]);

	default:
		ret = pmc_read(type, shift,
			       sen_info[type].ctrl[item], &data);
		if (ret)
			return ret;

		return sprintf(buf, "%d\n",
			       (data + sen_info[type].shift) *
			       sen_info[type].multi[item]);
	}

	return -EINVAL;
}

static struct sensor_device_attribute default_attr = {
	.dev_attr.attr.mode = 0444,
	.dev_attr.show = show,
};

static char devname[MAX_DEV][MAX_NAME];
static struct sensor_device_attribute devattrs[MAX_DEV];
static struct attribute *attrs[MAX_DEV];

static struct attribute_group group = {
	.attrs = attrs,
};

static const struct attribute_group *groups[] = {
	&group,
	NULL
};

static int hwmon_init(void)
{
	enum _sen_type type;
	u8 i, j, data[16];
	int sum = 0;
	int ret;

	for (type = VOLTAGE ; type <= CASEOPEN ; type++) {
		int cnt = 1;

		for (i = 0 ; i < sen_info[type].max ; i++) {
			if (pmc_read(type, i, 0x00, data) ||
			    (data[0] & 0x01) == 0)
				continue;

			memset(data, 0, sizeof(data));
			ret = pmc_read(type, i, 0x01, data);
			if (ret != 0 && ret != -EINVAL) {
				pr_info("read type id error\n");
				continue;
			}

			for (j = 0 ; j < ARRAY_SIZE(sen_info->item) ; j++) {
				if (sen_info[type].item[j][0] == 0)
					continue;

				devattrs[sum] = default_attr;
				attrs[sum] = &devattrs[sum].dev_attr.attr;
				devattrs[sum].dev_attr.attr.name = devname[sum];
				devattrs[sum].index = (type << 24) | (i << 16) |
						      (j    <<  8) | data[0];

				sprintf(devname[sum],
					"%s%d_%s",
					sen_info[type].name, cnt,
					sen_info[type].item[j]);

				if (++sum >= MAX_DEV)
					break;
			}
			cnt++;
		}
	}

	return sum;
}

static int hwmon_probe(struct platform_device *pdev)
{
	struct device *dev =  &pdev->dev;

	eiois200_dev = dev_get_drvdata(dev->parent);
	if (!eiois200_dev) {
		dev_err(dev, "Error contact eiois200_core\n");
		return -ENODEV;
	}

	if (!hwmon_init())
		return -ENODEV;

	hwmon_dev = devm_kzalloc(dev, sizeof(struct _hwmon_dev), GFP_KERNEL);

	hwmon_dev->regmap      = dev_get_regmap(dev->parent, NULL);
	if (!hwmon_dev->regmap)
		pr_err("Error grab regmap\n");

	platform_set_drvdata(pdev, hwmon_dev);

	hwmon_dev->dev = devm_hwmon_device_register_with_groups(dev,
								KBUILD_MODNAME,
								hwmon_dev,
								groups);
	return PTR_ERR_OR_ZERO(hwmon_dev->dev);
}

static struct platform_driver hwmon_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = KBUILD_MODNAME,
	}
};
module_platform_driver_probe(hwmon_driver, hwmon_probe);

MODULE_AUTHOR("Adavantech");
MODULE_DESCRIPTION("Hardware monitor driver for Advantech EIO-IS200 embedded controller");
MODULE_LICENSE("GPL");

