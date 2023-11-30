// SPDX-License-Identifier: GPL-2.0-only
/*
 * GPIO driver for Advantech EIO-IS200 Embedded controller.
 *
 * Copyright (C) 2023 Advantech Corporation. All rights reserved.
 */

#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/mfd/core.h>
#include <linux/gpio.h>
#include <linux/gpio/driver.h>
#include <linux/mfd/eiois200.h>

#define GPIO_MAX_PINS	48
#define GPIO_WRITE	0x18
#define GPIO_READ	0x19

struct eiois200_dev *eiois200_dev;

struct _gpio_dev {
	u64 avail;
	int max;
	struct regmap *regmap;
	struct gpio_chip chip;
} *gpio_dev = NULL;

struct {
	int size;
	bool write;
} ctrl_para[] = {
	{ 0x01, false }, { 0x00, false }, { 0x00, false }, { 0x02, false },
	{ 0x01, false }, { 0x00, false }, { 0x00, false }, { 0x00, false },
	{ 0x00, false }, { 0x00, false }, { 0x00, false }, { 0x00, false },
	{ 0x00, false }, { 0x00, false }, { 0x00, false }, { 0x00, false },
	{ 0x01, true  }, { 0x01, true  }, { 0x02, true  }, { 0x02, true  },
	{ 0x02, false }, { 0x10, false }
};

enum {
	GPIO_STATUS	 = 0,
	GPIO_GROUP_AVAIL = 3,
	GPIO_ERROR	 = 0x04,
	GPIO_PIN_DIR	 = 0x10,
	GPIO_PIN_LEVEL	 = 0x11,
	GPIO_GROUP_DIR	 = 0x12,
	GPIO_GROUP_LEVEL = 0x13,
	GPIO_MAPPING	 = 0x14,
	GPIO_NAME	 = 0x15
} gpio_ctrl;

struct {
	int group;
	int port;
} group_map[] = {
	{ 0, 0 }, { 0, 1 },
	{ 1, 0 }, { 1, 1 },
	{ 2, 0 }, { 2, 1 },
	{ 3, 0 }, { 3, 1 },
	{ 3, 2 }, { 3, 3 },
	{ 3, 4 }, { 3, 5 },
	{ 3, 6 }, { 3, 7 }
};

static int pmc_write(u8 ctrl, u8 dev_id, void *data)
{
	struct   pmc_op op = {
		 .cmd       = GPIO_WRITE,
		 .control   = ctrl,
		 .device_id = dev_id,
		 .payload   = (u8 *)data,
	};

	if (ctrl > ARRAY_SIZE(ctrl_para))
		return -ENOMEM;

	if (!ctrl_para[ctrl].write)
		return -EINVAL;

	op.size = ctrl_para[ctrl].size;

	return eiois200_core_pmc_operation(NULL, &op);
}

static int pmc_read(u8 ctrl, u8 dev_id, void *data)
{
	struct   pmc_op op = {
		 .cmd       = GPIO_READ,
		 .control   = ctrl,
		 .device_id = dev_id,
		 .payload   = (u8 *)data,
	};

	if (ctrl > ARRAY_SIZE(ctrl_para))
		return -ENOMEM;

	op.size = ctrl_para[ctrl].size;

	return eiois200_core_pmc_operation(NULL, &op);
}

static int get_dir(struct gpio_chip *chip, unsigned int offset)
{
	u8 dir;
	int ret;

	ret = pmc_read(GPIO_PIN_DIR, offset, &dir);
	if (ret)
		return ret;

	return dir ? 0 : 1;
}

static int dir_input(struct gpio_chip *chip, unsigned int offset)
{
	u8 dir = 0;

	return pmc_read(GPIO_PIN_DIR, offset, &dir);
}

static int dir_output(struct gpio_chip *chip, unsigned int offset, int value)
{
	u8 dir = 1;

	return pmc_write(GPIO_PIN_DIR, offset, &dir);
}

static int gpio_get(struct gpio_chip *chip, unsigned int offset)
{
	u8 level;
	int ret;

	ret = pmc_read(GPIO_PIN_LEVEL, offset, &level);
	if (ret)
		return ret;

	return level;
}

static void gpio_set(struct gpio_chip *chip, unsigned int offset, int value)
{
	pmc_write(GPIO_PIN_LEVEL, offset, &value);
}

static int check_support(void)
{
	u8  data;
	int ret;

	ret = pmc_read(GPIO_STATUS, 0, &data);
	if (!ret)
		return ret;

	if ((data & 0x01) == 0)
		return -ENOTSUPP;

	return 0;
}

static int check_pin(int pin)
{
	int ret;
	int group, bit;
	u16 data;

	/* Get pin mapping */
	ret = pmc_read(GPIO_MAPPING, pin, &data);
	if (ret)
		return ret;

	if ((data & 0xFF) > ARRAY_SIZE(group_map))
		return -EINVAL;

	group = group_map[data & 0xFF].group;
	bit   = data >> 8;

	/* Check mapped pin */
	ret = pmc_read(GPIO_GROUP_AVAIL, group, &data);
	if (ret)
		return ret;

	return data & BIT(bit) ? 0 : -ENOTSUPP;
}

static int gpio_init(void)
{
	int ret;
	int i;
	char str[GPIO_MAX_PINS + 1];

	memset(str, 0x30, sizeof(str));

	ret = check_support();
	if (ret) {
		pr_err("Error get GPIO support state\n");
		return ret;
	}

	gpio_dev->avail = 0;

	for (i = 0 ; i <  GPIO_MAX_PINS ; i++) {
		ret = check_pin(i);
		if (ret)
			continue;

		gpio_dev->avail |= BIT(i);
		gpio_dev->max = i;
		str[GPIO_MAX_PINS - i] = '1';
	}

	pr_info("GPIO pins=%s\n", str);

	return gpio_dev->max ? 0 : -ENOTSUPP;
}

static const struct gpio_chip eiois200_gpio_chip = {
	.label		  = KBUILD_MODNAME,
	.owner		  = THIS_MODULE,
	.direction_input  = dir_input,
	.get		  = gpio_get,
	.direction_output = dir_output,
	.set		  = gpio_set,
	.get_direction	  = get_dir,
	.base		  = -1,
	.can_sleep	  = true,
};

static int gpio_probe(struct platform_device *pdev)
{
	struct device *dev =  &pdev->dev;

	eiois200_dev = dev_get_drvdata(dev->parent);
	if (!eiois200_dev) {
		dev_err(dev, "Error contact eiois200_core\n");
		return -ENOMEM;
	}

	gpio_dev = devm_kzalloc(dev, sizeof(struct _gpio_dev), GFP_KERNEL);

	if (gpio_init())
		return -EIO;

	gpio_dev->regmap      = dev_get_regmap(dev->parent, NULL);
	gpio_dev->chip	      = eiois200_gpio_chip;
	gpio_dev->chip.parent = dev->parent;
	gpio_dev->chip.ngpio  = gpio_dev->max;

	if (!gpio_dev->regmap)
		pr_err("Error grab regmap\n");

	return devm_gpiochip_add_data(dev, &gpio_dev->chip, gpio_dev);
}

static struct platform_driver gpio_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = KBUILD_MODNAME,
	}
};
module_platform_driver_probe(gpio_driver, gpio_probe);

MODULE_AUTHOR("Adavantech");
MODULE_DESCRIPTION("GPIO driver for Advantech EIO-IS200 embedded controller");
MODULE_LICENSE("GPL");
