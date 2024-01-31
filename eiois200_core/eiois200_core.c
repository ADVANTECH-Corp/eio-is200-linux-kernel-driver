// SPDX-License-Identifier: GPL-2.0-only
/*
 * Advantech EIO-IS200 Series EC base Driver
 *
 * This driver provides an interface to access the EIO-IS200 Series EC
 * firmware via its own Power Management Channel (PMC) for subdrivers:
 *
 * A system may have one or two independent EIO-IS200s.
 *
 * Copyright (C) 2023 Advantech Co., Ltd.
 * Author: Wenkai <advantech.susiteam@gmail.com>
 */

#include <linux/delay.h>
#include <linux/isa.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/regmap.h>
#include <linux/sysfs.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/mfd/eiois200.h>

#define TIMEOUT_MAX     (10 * USEC_PER_SEC)
#define TIMEOUT_MIN	200
#define DEFAULT_TIMEOUT 5000

/**
 * Timeout: Default timeout in microseconds when a PMC command's
 * timeout is unspecified. PMC command responses typically range
 * from 200us to 2ms. 5ms is quite a safe value for timeout. In
 * In some cases, responses are longer. In such situations, please
 * adding the timeout parameter loading related sub-drivers or
 * this core driver (not recommended).
 */
static uint timeout = DEFAULT_TIMEOUT;
module_param(timeout, uint, 0444);
MODULE_PARM_DESC(timeout,
		 "Default PMC command timeout in usec.\n");

struct eiois200_dev_port {
	u16 idx_port;
	u16 data_port;
};

static struct eiois200_dev_port pnp_port[] = {
	{ .idx_port = EIOIS200_PNP_INDEX,     .data_port = EIOIS200_PNP_DATA     },
	{ .idx_port = EIOIS200_SUB_PNP_INDEX, .data_port = EIOIS200_SUB_PNP_DATA },
};

static struct eiois200_dev *eiois200_dev;
static struct regmap *regmap_is200;

static struct mfd_cell mfd_devs[] = {
	{ .name = "eiois200_wdt"     },
	{ .name = "gpio_eiois200"    },
	{ .name = "eiois200_hwmon"   },
	{ .name = "i2c_eiois200"     },
	{ .name = "eiois200_thermal" },
	{ .name = "eiois200_fan" },
	{ .name = "eiois200_bl"	     },
};

static struct regmap_range is200_range[] = {
	 regmap_reg_range(EIOIS200_PNP_INDEX,	  EIOIS200_PNP_DATA),
	 regmap_reg_range(EIOIS200_SUB_PNP_INDEX, EIOIS200_SUB_PNP_DATA),
	 regmap_reg_range(0x200,		  0x3FF),
};

static const struct regmap_access_table volatile_regs = {
	.yes_ranges   = is200_range,
	.n_yes_ranges = ARRAY_SIZE(is200_range),
};

#if KERNEL_VERSION(6, 0, 0) <= LINUX_VERSION_CODE
static const struct regmap_config pnp_regmap_config = {
	.name		= "eiois200_core",
	.reg_bits	= 16,
	.val_bits	= 8,
	.volatile_table = &volatile_regs,
	.io_port	= true,
	.cache_type	= REGCACHE_NONE,
};
#elif KERNEL_VERSION(5, 19, 0) <= LINUX_VERSION_CODE

static int read(void *context, const void *reg_buf, size_t reg_size,
		void *val_buf, size_t val_size)
{
	u16 *port = (u16 *)reg_buf;
	u8 *val = (u8 *)val_buf;

	*val = inb(*port);

	return 0;
}

static int write(void *context, const void *data, size_t count)
{
	struct {
		u16 reg : 16;
		u8 val : 8;
	} *p = (void *)data;

	outb(p->val, p->reg);
	return 0;
}

static int reg_read(void *context, unsigned int reg, unsigned int *val)
{
	*(u8 *)val = inb(reg);
	return 0;
}

static int reg_write(void *context, unsigned int reg, unsigned int val)
{
	outb(val, reg);
	return 0;
}

static const struct regmap_config pnp_regmap_config = {
	.name		= "eiois200_core",
	.reg_bits	= 16,
	.val_bits	= 8,
	.volatile_table = &volatile_regs,
	.read		= read,
	.write		= write,
	.reg_read	= reg_read,
	.reg_write	= reg_write,
};
#else
//#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
static int reg_read(void *context, unsigned int reg, unsigned int *val)
{
	*val = (uint)inb(reg);
	return 0;
}

static int reg_write(void *context, unsigned int reg, unsigned int val)
{
	outb(val, reg);
	return 0;
}

static const struct regmap_config pnp_regmap_config = {
	.name		= "eiois200_core",
	.reg_bits	= 16,
	.val_bits	= 8,
	.volatile_table = &volatile_regs,
	.reg_read	= reg_read,
	.reg_write	= reg_write,
	.fast_io	= true,
};

//#else
//	#error "Unsupported kernel version! This driver requires at least Linux kernel 5.10.0"
#endif

static struct {
	char name[32];
	int  cmd;
	int  ctrl;
	int  dev;
	int  size;
	enum {
		HEX,
		NUMBER,
		PNP_ID,
	} type;

} attrs[] = {
	{ "board_name",		0x53, 0x10, 0, 16 },
	{ "board_serial",	0x53, 0x1F, 0, 16 },
	{ "board_manufacturer", 0x53, 0x11, 0, 16 },
	{ "board_id",		0x53, 0x1E, 0,  4 },
	{ "firmware_version",	0x53, 0x22, 0, 16 },
	{ "firmware_build",	0x53, 0x23, 0, 26 },
	{ "firmware_date",	0x53, 0x24, 0, 16 },
	{ "chip_id",		0x53, 0x12, 0, 12 },
	{ "chip_detect",	0x53, 0x15, 0, 12 },
	{ "platform_type",	0x53, 0x13, 0, 16 },
	{ "platform_revision",	0x53, 0x14, 0,  4 },
	{ "eapi_version",	0x53, 0x30, 0,  4 },
	{ "eapi_id",		0x53, 0x31, 0,  4 },
	{ "boot_count",		0x55, 0x10, 0,  4, NUMBER },
	{ "powerup_hour",	0x55, 0x11, 0,  4, NUMBER },
	{ "pnp_id",		0x53, 0x04, 0x68,  4, PNP_ID },
};

void __iomem *iomem;

static ssize_t info_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	uint i;

	for (i = 0; i < ARRAY_SIZE(attrs); i++) {
		int ret;
		char str[32] = "";
		int val;

		struct pmc_op op = {
			.cmd     = attrs[i].cmd,
			.control = attrs[i].ctrl,
			.device_id = attrs[i].dev,
			.payload = (u8 *)str,
			.size    = attrs[i].size,
		};

		if (strcmp(attr->attr.name, attrs[i].name))
			continue;

		ret = eiois200_core_pmc_operation(dev, &op);
		if (ret)
			return ret;

		if (attrs[i].size != 4)
			return sprintf(buf, "%s\n", str);

		val = *(u32 *)str;

		if (attrs[i].type == HEX)
			return sprintf(buf, "0x%X\n", val);

		if (attrs[i].type == NUMBER)
			return sprintf(buf, "%d\n", val);

		/* Should be pnp_id */
		return sprintf(buf, "%c%c%c, %X\n",
			       (val >> 14 & 0x3F) + 0x40,
			       ((val >> 9 & 0x18) | (val >> 25 & 0x07)) + 0x40,
			       (val >> 20 & 0x1F) + 0x40,
			       val & 0xFFF);
	}

	return -EINVAL;
}

#define PMC_DEVICE_ATTR_RO(_name) \
static ssize_t _name##_show(struct device *dev, struct device_attribute *attr, char *buf) \
{ \
	return info_show(dev, attr, buf); \
} \
static DEVICE_ATTR_RO(_name)

PMC_DEVICE_ATTR_RO(board_name);
PMC_DEVICE_ATTR_RO(board_serial);
PMC_DEVICE_ATTR_RO(board_manufacturer);
PMC_DEVICE_ATTR_RO(firmware_version);
PMC_DEVICE_ATTR_RO(firmware_build);
PMC_DEVICE_ATTR_RO(firmware_date);
PMC_DEVICE_ATTR_RO(chip_id);
PMC_DEVICE_ATTR_RO(chip_detect);
PMC_DEVICE_ATTR_RO(platform_type);
PMC_DEVICE_ATTR_RO(platform_revision);
PMC_DEVICE_ATTR_RO(board_id);
PMC_DEVICE_ATTR_RO(eapi_version);
PMC_DEVICE_ATTR_RO(eapi_id);
PMC_DEVICE_ATTR_RO(boot_count);
PMC_DEVICE_ATTR_RO(powerup_hour);
PMC_DEVICE_ATTR_RO(pnp_id);

static struct attribute *pmc_attrs[] = {
	&dev_attr_board_name.attr,
	&dev_attr_board_serial.attr,
	&dev_attr_board_manufacturer.attr,
	&dev_attr_firmware_version.attr,
	&dev_attr_firmware_build.attr,
	&dev_attr_firmware_date.attr,
	&dev_attr_chip_id.attr,
	&dev_attr_chip_detect.attr,
	&dev_attr_platform_type.attr,
	&dev_attr_platform_revision.attr,
	&dev_attr_board_id.attr,
	&dev_attr_eapi_version.attr,
	&dev_attr_eapi_id.attr,
	&dev_attr_boot_count.attr,
	&dev_attr_powerup_hour.attr,
	&dev_attr_pnp_id.attr,
	NULL
};

ATTRIBUTE_GROUPS(pmc);

/* Following are EIO-IS200 PNP IO port access functions */
static int is200_pnp_read(struct device *dev,
			  struct eiois200_dev_port *port,
			  u8 idx)
{
	int val;

	if (regmap_write(regmap_is200, port->idx_port, idx))
		dev_err(dev, "Error port write 0x%X\n", port->idx_port);

	if (regmap_read(regmap_is200, port->data_port, &val))
		dev_err(dev, "Error port read 0x%X\n", port->data_port);

	return val;
}

static void is200_pnp_write(struct device *dev,
			    struct eiois200_dev_port *port,
			    u8 idx,
			    u8 data)
{
	if (regmap_write(regmap_is200, port->idx_port, idx) ||
	    regmap_write(regmap_is200, port->data_port, data))
		dev_err(dev, "Error port write 0x%X %X\n",
			port->idx_port, port->data_port);
}

static void is200_pnp_enter(struct device *dev,
			    struct eiois200_dev_port *port)
{
	/* Write 0x87 to index port twice to unlock IO port */
	if (regmap_write(regmap_is200, port->idx_port, EIOIS200_EXT_MODE_ENTER) ||
	    regmap_write(regmap_is200, port->idx_port, EIOIS200_EXT_MODE_ENTER))
		dev_err(dev, "Error port write 0x%X\n", port->idx_port);
}

static void is200_pnp_leave(struct device *dev,
			    struct eiois200_dev_port *port)
{
	/* Write 0xAA to index port once to lock IO port */
	if (regmap_write(regmap_is200, port->idx_port, EIOIS200_EXT_MODE_EXIT))
		dev_err(dev, "Error port write 0x%X\n", port->idx_port);
}

/* Following are EIO-IS200 IO port access functions for PMC command */
static int pmc_write_data(struct device *dev,
			  int id,
			  u8 value,
			  u16 timeout)
{
	int ret;

	if (WAIT_IBF(dev, id, timeout))
		return -ETIME;

	ret = regmap_write(regmap_is200, eiois200_dev->pmc[id].data, value);
	if (ret)
		dev_err(dev, "Error PMC write %X:%X\n",
			eiois200_dev->pmc[id].data, value);

	return ret;
}

static int pmc_write_cmd(struct device *dev,
			 int id,
			 u8 value,
			 u16 timeout)
{
	int ret;

	if (WAIT_IBF(dev, id, timeout))
		return -ETIME;

	ret = regmap_write(regmap_is200, eiois200_dev->pmc[id].cmd, value);
	if (ret)
		dev_err(dev, "Error PMC write %X:%X\n",
			eiois200_dev->pmc[id].data, value);

	return ret;
}

static int pmc_read_data(struct device *dev,
			 int id,
			 u8 *value,
			 u16 timeout)
{
	int val, ret;

	if (WAIT_OBF(dev, id, timeout))
		return -ETIME;

	ret = regmap_read(regmap_is200, eiois200_dev->pmc[id].data, &val);
	if (ret)
		dev_err(dev, "Error PMC read %X\n",
			eiois200_dev->pmc[id].data);
	else
		*value = val & 0xFF;

	return ret;
}

/**
 * pmc_read_status - Read the pmc current status
 * @dev:	The device structure pointer.
 * @id:		0 for main chip, 1 for sub chip.
 * Returns:	The current status read from the PMC,
 *		or 0 if there was an error.
 */
static int pmc_read_status(struct device *dev, int id)
{
	int val;

	if (regmap_read(regmap_is200, eiois200_dev->pmc[id].data, &val)) {
		dev_err(dev, "Error PMC read %X\n", eiois200_dev->pmc[id].data);
		return 0;
	}

	return val;
}

static void pmc_clear(struct device *dev, int id)
{
	int val;

	/* Check if input buffer blocked */
	if ((pmc_read_status(dev, id) & EIOIS200_PMC_STATUS_IBF) == 0)
		return;

	/* Read out previous garbage */
	if (regmap_read(regmap_is200, eiois200_dev->pmc[id].data, &val))
		dev_err(dev, "Error pmc clear\n");

	usleep_range(10, 100);
}

/**
 * eiois200_core_pmc_wait - Wait for input / output buffer to be ready.
 * @dev:		The device structure pointer.
 * @id:			0 for main chip, 1 for sub chip.
 * @wait:		%PMC_WAIT_INPUT or %PMC_WAIT_OUTPUT.
 *			%PMC_WAIT_INPUT for waiting input buffer data ready.
 *			%PMC_WAIT_OUTPUT for waiting output buffer empty.
 * max_duration:	The timeout value in usec.
 */
int eiois200_core_pmc_wait(struct device *dev,
			   int id,
			   enum eiois200_pmc_wait wait,
			   uint max_duration)
{
	int ret;
	uint val;
	u32 cnt = 0;
	int new_timeout = max_duration ? max_duration : timeout;
	ktime_t time_end = ktime_add_us(ktime_get(), new_timeout);

	if (new_timeout < TIMEOUT_MIN || new_timeout > TIMEOUT_MAX) {
		dev_err(dev,
			"Error timeout value: %dus. Timeout value should between %d and %ld\n",
			new_timeout, TIMEOUT_MIN, TIMEOUT_MAX);
		return -ETIME;
	}

	do {
		ret = regmap_read(regmap_is200,
				  eiois200_dev->pmc[id].status,
				  &val);
		if (ret)
			return ret;

		if (wait == PMC_WAIT_INPUT) {
			if ((val & EIOIS200_PMC_STATUS_IBF) == 0)
				return 0;
		} else {
			if ((val & EIOIS200_PMC_STATUS_OBF) != 0)
				return 0;
		}

		/* Incremental delay */
		cnt += 10;

		usleep_range(cnt, 2 * cnt);

	} while (ktime_before(ktime_get(), time_end));

	return -ETIME;
}
EXPORT_SYMBOL_GPL(eiois200_core_pmc_wait);

/**
 * eiois200_core_pmc_operation - Execute a PMC command
 * @dev:	The device structure pointer.
 * @op:		Pointer to an PMC command.
 */
int eiois200_core_pmc_operation(struct device *dev,
				struct pmc_op *op)
{
	u8	i;
	int	ret;
	bool	read_cmd = op->cmd & EIOIS200_FLAG_PMC_READ;
	ktime_t t = ktime_get();

	mutex_lock(&eiois200_dev->mutex);

	pmc_clear(dev, op->chip);

	ret = pmc_write_cmd(dev, op->chip, op->cmd, op->timeout);
	if (ret)
		goto err;

	ret = pmc_write_data(dev, op->chip, op->control, op->timeout);
	if (ret)
		goto err;

	ret = pmc_write_data(dev, op->chip, op->device_id, op->timeout);
	if (ret)
		goto err;

	ret = pmc_write_data(dev, op->chip, op->size, op->timeout);
	if (ret)
		goto err;

	for (i = 0; i < op->size; i++) {
		if (read_cmd)
			ret = pmc_read_data(dev, op->chip,
					    &op->payload[i], op->timeout);
		else
			ret = pmc_write_data(dev, op->chip,
					     op->payload[i], op->timeout);

		if (ret)
			goto err;
	}

	mutex_unlock(&eiois200_dev->mutex);

	return 0;

err:
	mutex_unlock(&eiois200_dev->mutex);

	dev_err(dev, "PMC error duration:%lldus", ktime_to_us(ktime_sub(ktime_get(), t)));
	dev_err(dev, ".cmd=0x%02X, .ctrl=0x%02X .id=0x%02X, .size=0x%02X .data=0x%02X%02X",
		op->cmd, op->control, op->device_id,
	       op->size, op->payload[0], op->payload[1]);

	return ret;
}
EXPORT_SYMBOL_GPL(eiois200_core_pmc_operation);

static int get_pmc_port(struct device *dev,
			int id,
			struct eiois200_dev_port *port)
{
	struct _pmc_port *pmc = &eiois200_dev->pmc[id];

	is200_pnp_enter(dev, port);

	/* Switch to PMC device page */
	is200_pnp_write(dev, port, EIOIS200_LDN, EIOIS200_LDN_PMC1);

	/* Active this device */
	is200_pnp_write(dev, port, EIOIS200_LDAR, EIOIS200_LDAR_LDACT);

	/* Get PMC cmd and data port */
	pmc->data  = is200_pnp_read(dev, port, EIOIS200_IOBA0H) << 8;
	pmc->data |= is200_pnp_read(dev, port, EIOIS200_IOBA0L);
	pmc->cmd   = is200_pnp_read(dev, port, EIOIS200_IOBA1H) << 8;
	pmc->cmd  |= is200_pnp_read(dev, port, EIOIS200_IOBA1L);

	/* Disable IRQ */
	is200_pnp_write(dev, port, EIOIS200_IRQCTRL, 0);

	is200_pnp_leave(dev, port);

	/* Make sure IO ports are not occupied */
	if (!devm_request_region(dev, pmc->data, 2, KBUILD_MODNAME)) {
		dev_err(dev, "Request region %X error\n", pmc->data);
		return -EBUSY;
	}

	return 0;
}

static int eiois200_init(struct device *dev)
{
	u16  chip_id = 0;
	u8   tmp = 0;
	int  chip = 0;
	int  ret = -ENOMEM;

	for (chip = 0; chip < ARRAY_SIZE(pnp_port); chip++) {
		struct eiois200_dev_port *port = pnp_port + chip;

		if (!devm_request_region(dev,
					 pnp_port[chip].idx_port,
					 pnp_port[chip].data_port -
					 pnp_port[chip].idx_port,
					 KBUILD_MODNAME))
			continue;

		is200_pnp_enter(dev, port);

		chip_id  = is200_pnp_read(dev, port, EIOIS200_CHIPID1) << 8;
		chip_id |= is200_pnp_read(dev, port, EIOIS200_CHIPID2);

		if (chip_id != EIOIS200_CHIPID &&
		    chip_id != EIO201_211_CHIPID)
			continue;

		/* Turn on the enable flag */
		tmp = is200_pnp_read(dev, port, EIOIS200_SIOCTRL);
		tmp |= EIOIS200_SIOCTRL_SIOEN;

		is200_pnp_write(dev, port, EIOIS200_SIOCTRL, tmp);

		is200_pnp_leave(dev, port);

		ret = get_pmc_port(dev, chip, port);
		if (ret)
			return ret;

		if (chip == 0)
			eiois200_dev->flag |= EIOIS200_F_CHIP_EXIST;
		else
			eiois200_dev->flag |= EIOIS200_F_SUB_CHIP_EXIST;
	}

	return ret;
}

/**
 * acpiram_access - Read ACPI information stored in the EC
 * @dev:	The device structure pointer.
 * @offset:	The offset of information.
 * Returns:	The value read from the PMC, or 0 if there was an error.
 */
static uint8_t acpiram_access(struct device *dev, uint8_t offset)
{
	u8  val;
	int ret;
	int timeout = 0;

	/* We only store information on primary EC */
	int chip = 0;

	mutex_lock(&eiois200_dev->mutex);

	pmc_clear(dev, chip);

	ret = pmc_write_cmd(dev, chip, EIOIS200_PMC_CMD_ACPIRAM_READ, timeout);
	if (ret)
		goto err;

	ret = pmc_write_data(dev, chip, offset, timeout);
	if (ret)
		goto err;

	ret = pmc_write_data(dev, chip, sizeof(val), timeout);
	if (ret)
		goto err;

	ret = pmc_read_data(dev, chip, &val, timeout);
	if (ret)
		goto err;

err:
	mutex_unlock(&eiois200_dev->mutex);
	return ret ? 0 : val;
}

static int firmware_code_base(struct device *dev)
{
	u8 ic_vendor, ic_code, code_base;

	ic_vendor = acpiram_access(dev, EIOIS200_ACPIRAM_ICVENDOR);
	ic_code   = acpiram_access(dev, EIOIS200_ACPIRAM_ICCODE);
	code_base = acpiram_access(dev, EIOIS200_ACPIRAM_CODEBASE);

	if (ic_vendor != 'R')
		return -ENODEV;

	if (ic_code != EIOIS200_ICCODE &&
	    ic_code != EIO201_ICCODE   &&
	    ic_code != EIO211_ICCODE)
		goto err;

	if (code_base == EIOIS200_ACPIRAM_CODEBASE_NEW) {
		eiois200_dev->flag |= EIOIS200_F_NEW_CODE_BASE;
		return 0;
	}

	if (code_base == 0 && (ic_code != EIO201_ICCODE &&
			       ic_code != EIO211_ICCODE)) {
		dev_info(dev, "Old code base not supported, yet.");
		return -ENODEV;
	}

 err:
	/* Codebase error. This should only happen on firmware error. */
	dev_err(dev, "Codebase check fail: vendor: 0x%X, code: 0x%X, base: 0x%X\n",
		ic_vendor, ic_code, code_base);
	return -ENODEV;
}

static int eiois200_probe(struct device *dev, unsigned int id)
{
	int  ret = 0;

	iomem = devm_ioport_map(dev, 0, EIOIS200_SUB_PNP_DATA + 1);
	if (IS_ERR(iomem))
		return -ENOMEM;

#if KERNEL_VERSION(5, 19, 0) <= LINUX_VERSION_CODE
	regmap_is200 = devm_regmap_init_mmio(dev, iomem, &pnp_regmap_config);
#else
	regmap_is200 = devm_regmap_init(dev, NULL, eiois200_dev, &pnp_regmap_config);
#endif
	if (IS_ERR(regmap_is200))
		return -ENOMEM;

	eiois200_dev = devm_kzalloc(dev, sizeof(*eiois200_dev), GFP_KERNEL);
	if (!eiois200_dev)
		return -ENOMEM;

	mutex_init(&eiois200_dev->mutex);

	if (eiois200_init(dev)) {
		dev_dbg(dev, "No device found\n");
		return -ENODEV;
	}

	if (firmware_code_base(dev)) {
		dev_err(dev, "Chip code base check fail\n");
		return -EIO;
	}

	dev_set_drvdata(dev, eiois200_dev);

	ret = devm_mfd_add_devices(dev, PLATFORM_DEVID_NONE, mfd_devs,
				   ARRAY_SIZE(mfd_devs),
				   NULL, 0, NULL);
	if (ret)
		dev_err(dev, "Cannot register child devices (error = %d)\n", ret);

	dev_dbg(dev, "Module insert completed\n");

	return 0;
}

static struct isa_driver eiois200_driver = {
	.probe    = eiois200_probe,

	.driver = {
		.name = "eiois200_core",
		.dev_groups = pmc_groups,
	},
};
module_isa_driver(eiois200_driver, 1);

MODULE_AUTHOR("Wenkai <advantech.susiteam@gmail.com>");
MODULE_DESCRIPTION("Advantech EIO-IS200 series EC core driver");
MODULE_LICENSE("GPL v2");
