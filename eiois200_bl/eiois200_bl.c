// SPDX-License-Identifier: GPL-2.0-only
/*
 * Backlight driver for Advantech EIO-IS200 Embedded controller.
 *
 * Copyright (C) 2023 Advantech Corporation. All rights reserved.
 */

#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/backlight.h>
#include <linux/mfd/eiois200.h>

#define PMC_BL_WRITE		0x20
#define PMC_BL_READ		0x21

#define BL_CTRL_STATUS		0x00
#define BL_CTRL_ENABLE		0x12
#define BL_CTRL_ENABLE_INVERT	0x13
#define BL_CTRL_DUTY		0x14
#define BL_CTRL_INVERT		0x15
#define BL_CTRL_FREQ		0x16

#define BL_MAX			2

#define BL_STATUS_AVAIL		0x01
#define BL_ENABLE_OFF		0x00
#define BL_ENABLE_ON		0x01
#define BL_ENABLE_AUTO		BIT(1)

#define USE_DEFAULT		-1
#define THERMAL_MAX		100

union bl_status {
	struct {
		u8 avail : 1;
		u8 pwm_dc : 1;
		u8 pwm_src : 1;
		u8 bri_invert : 1;
		u8 enable_pin_supported : 1;
		u8 bl_power_invert : 1;
		u8 enable_pin_enabled : 1;
		u8 firmware_error : 1;
	};
	u8 value;
};

static uint bri_freq = USE_DEFAULT;
module_param(bri_freq, uint, 0444);
MODULE_PARM_DESC(bri_freq, "Setup backlight PWM frequency.\n");

static int bri_invert = USE_DEFAULT;
module_param(bri_invert, int, 0444);
MODULE_PARM_DESC(bri_invert, "Setup backlight PWM polarity.\n");

static int bl_power_invert = USE_DEFAULT;
module_param(bl_power_invert, int, 0444);
MODULE_PARM_DESC(bl_power_invert, "Setup backlight enable pin polarity.\n");

static int pmc_cmd(struct device *dev, u8 cmd, u8 ctrl, u8 id, void *data)
{
	struct pmc_op op = {
		.cmd       = cmd,
		.control   = ctrl,
		.device_id = id,
		.size	   = ctrl == BL_CTRL_FREQ ? 4 : 1,
		.payload   = (u8 *)data,
	};

	return eiois200_core_pmc_operation(dev, &op);
}

#define PMC_WRITE(dev, ctrl, id, data) \
	pmc_cmd(dev, PMC_BL_WRITE, ctrl, id, data)

#define PMC_READ(dev, ctrl, id, data) \
	pmc_cmd(dev, PMC_BL_READ, ctrl, id, data)

static int bl_update_status(struct backlight_device *bl)
{
	int ret;
	struct device *dev = &bl->dev;
	long id = (long)bl_get_data(bl);
	u8 sw = bl->props.power == FB_BLANK_UNBLANK;

	/* Setup PWM duty */
	ret = PMC_WRITE(dev, BL_CTRL_DUTY, id, &bl->props.brightness);
	if (ret)
		return ret;

	/* Setup backlight enable pin */
	return PMC_WRITE(dev, BL_CTRL_ENABLE, id, &sw);
}

static int bl_get_brightness(struct backlight_device *bl)
{
	u8 duty = 0;
	int id = (long)bl_get_data(bl);
	int ret;

	ret = PMC_READ(&bl->dev, BL_CTRL_DUTY, id, &duty);
	if (ret)
		return ret;

	return duty;
}

static const struct backlight_ops bl_ops = {
	.get_brightness = bl_get_brightness,
	.update_status	= bl_update_status,
	.options	= BL_CORE_SUSPENDRESUME,
};

static int bl_init(struct device *dev, int id, 
		   struct backlight_properties *props)
{
	int ret = 0;
	u8 enabled;
	union bl_status status = { .value = 0 };

	/* Check EC supported backlight or not */
	ret = PMC_READ(dev, BL_CTRL_STATUS, id, &status);
	if (ret)
		return ret;

	if (!status.avail) {
		dev_dbg(dev, "eiois200_bl%d hardware report disabled.\n", id);
		return -ENXIO;
	}

	/* Read duty */
	ret = PMC_READ(dev, BL_CTRL_DUTY, id, &props->brightness);	

	/* Invert PWM */
	dev_dbg(dev, "bri_invert=%d\n", bri_invert);
	if (bri_invert > USE_DEFAULT)
		ret = PMC_WRITE(dev, BL_CTRL_INVERT, id, &bri_invert);
	
	bri_invert = 0;
	ret = PMC_READ(dev, BL_CTRL_INVERT, id, &bri_invert);	

	/* Setup freq */
	dev_dbg(dev, "bri_freq=%d\n", bri_freq);
	if (bri_freq != USE_DEFAULT) 
		ret = PMC_WRITE(dev, BL_CTRL_FREQ, id, &bri_freq);
	
	PMC_READ(dev, BL_CTRL_FREQ, id, &bri_freq);

	/* Invert enable pin*/
	dev_dbg(dev, "bl_power_invert=%d\n", bl_power_invert);
	if (bl_power_invert >= USE_DEFAULT)
		ret = PMC_WRITE(dev, BL_CTRL_ENABLE_INVERT,
			       id, &bl_power_invert);
			       
	bl_power_invert = 0;
	ret = PMC_READ(dev, BL_CTRL_ENABLE_INVERT, id, &bl_power_invert);

	/* Read power state */
	ret = PMC_READ(dev, BL_CTRL_ENABLE, id, &enabled);
	if (ret)
		return ret;

	props->power = enabled? FB_BLANK_UNBLANK : FB_BLANK_NORMAL ;

	return ret;
}

static struct backlight_properties props = {
	.type = BACKLIGHT_RAW,
	.max_brightness = THERMAL_MAX,
	.power = FB_BLANK_UNBLANK,
	.brightness = THERMAL_MAX,
};

static int bl_probe(struct platform_device *pdev)
{
	long id;
	int ret = -ENXIO;
	struct device *dev =  &pdev->dev;
	struct backlight_device *bl;

	/* Confirm if eiois200_core exist */
	if (!dev_get_drvdata(dev->parent))
		return dev_err_probe(dev, -ENOMEM,
				     "Error contact eiois200_core %d\n", ret);

	/* Init and register 2 backlights */
	for (id = 0; id < BL_MAX; id++) {
		char name[256];

		ret = bl_init(dev, id, &props);
		if (ret)
			continue;

		sprintf (name, "%s%ld", pdev->name, id);
		bl = devm_backlight_device_register(dev, name, dev, (void *)id,
						    &bl_ops, &props);
		if (IS_ERR(bl))
			return PTR_ERR(bl);
		
		dev_dbg(dev, "%s registered\n", name);
	}

	return 0;
}

static struct platform_driver bl_driver = {
	.driver = {
		.name  = "eiois200_bl",
	}
};
module_platform_driver_probe(bl_driver, bl_probe);

MODULE_AUTHOR("Adavantech");
MODULE_DESCRIPTION("GPIO driver for Advantech EIO-IS200 embedded controller");
MODULE_LICENSE("GPL v2");
