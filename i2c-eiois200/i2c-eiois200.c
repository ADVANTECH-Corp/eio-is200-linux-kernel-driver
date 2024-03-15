// SPDX-License-Identifier: GPL-2.0-only
/*
 * I2C and SMBus driver of EIO-IS200 embedded driver
 *
 * Copyright (C) 2023 Advantech Co., Ltd.
 * Author: Wenkai <advantech.susiteam@gmail.com>
 */

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/mfd/eiois200.h>
#include <linux/version.h>


#define SUPPORTED_COMMON (I2C_FUNC_I2C | \
			  I2C_FUNC_SMBUS_QUICK | \
			  I2C_FUNC_SMBUS_BYTE | \
			  I2C_FUNC_SMBUS_BYTE_DATA | \
			  I2C_FUNC_SMBUS_WORD_DATA | \
			  I2C_FUNC_SMBUS_I2C_BLOCK)
#define SUPPORTED_SMB	(SUPPORTED_COMMON | I2C_FUNC_SMBUS_BLOCK_DATA)
#define SUPPORTED_I2C	(SUPPORTED_COMMON | I2C_FUNC_10BIT_ADDR)

#define MAX_I2C_SMB		4

#define REG_PNP_INDEX		0x299
#define REG_PNP_DATA		0x29A
#define REG_SUB_PNP_INDEX	0x499
#define REG_SUB_PNP_DATA	0x49A
#define REG_EXT_MODE_ENTER	0x87
#define REG_EXT_MODE_EXIT	0xAA
#define REG_LDN			0x07

#define LDN_I2C0		0x20
#define LDN_I2C1		0x21
#define LDN_SMBUS0		0x22
#define LDN_SMBUS1		0x23

#define REG_BASE_HI		0x60
#define REG_BASE_LO		0x61

#define I2C_REG_CTRL		0x00
#define I2C_CTRL_STOP		BIT(1)

#define I2C_REG_STAT		0x01
#define I2C_STAT_RXREADY	BIT(6)
#define I2C_STAT_TXDONE		BIT(5)
#define I2C_STAT_NAK_ERR	BIT(4)
#define I2C_STAT_ARL_ERR	BIT(3)
#define I2C_STAT_SLV_STP	BIT(2)
#define I2C_STAT_BUSY		BIT(1)
#define I2C_STAT_MST_SLV	BIT(0)

#define I2C_REG_MYADDR		0x02
#define I2C_REG_ADDR		0x03
#define I2C_REG_DATA		0x04
#define I2C_REG_PRESCALE1	0x05
#define I2C_REG_PRESCALE2	0x06

#define I2C_REG_ECTRL		0x07
#define I2C_ECTRL_RST		BIT(7)

#define I2C_REG_SEM		0x08
#define I2C_SEM_INUSE		BIT(1)

#define SMB_REG_HC2		0x0C

#define SMB_REG_HS		0x00
#define SMB_HS_BUSY		BIT(0)
#define SMB_HS_FINISH		BIT(1)
#define SMB_HS_ARL_ERR		BIT(3)
#define SMB_HS_FAILED		BIT(4)
#define SMB_HS_RX_READY		BIT(5)
#define SMB_HS_INUSE		BIT(6)
#define SMB_HS_TX_DONE		BIT(7)

#define SMB_REG_HS2		0x01
#define SMB_HS2_HNOTIFY		BIT(0)
#define SMB_HS2_PEC_ERR		BIT(1)
#define SMB_HS2_NACK_ERR	BIT(2)
#define SMB_HS2_ALERT_STS	BIT(3)
#define SMB_HS2_TO_ERR		BIT(4)
#define SMB_HS2_SSTOP_STS	BIT(5)
#define SMB_HS2_STX_REQ		BIT(6)
#define SMB_HS2_SMODE		BIT(7)

#define SMB_REG_HC		0x02
#define SMB_HC_I2C_NACKEN	BIT(0)
#define SMB_HC_KILL		BIT(1)
#define SMB_HC_CMD_SHIFT	2
#define SMB_HC_LAST_BYTE	BIT(5)
#define SMB_HC_START		BIT(6)
#define SMB_HC_PEC_EN		BIT(7)

#define SMB_REG_HCMD		0x03
#define SMB_REG_HADDR		0x04
#define SMB_REG_HD0		0x05
#define SMB_REG_HD1		0x06
#define SMB_REG_HBLOCK		0x07
#define SMB_REG_HPEC		0x08
#define SMB_REG_SADDR		0x09
#define SMB_REG_SD0		0x0A
#define SMB_REG_SD1		0x0B

#define SMB_REG_HC2		0x0C
#define SMB_HC2_HNOTIFY_DIS	BIT(0)
#define SMB_HC2_I2C_EN		BIT(1)
#define SMB_HC2_AAPEC		BIT(2)
#define SMB_HC2_E32B		BIT(3)
#define SMB_HC2_SRESET		BIT(7)

#define SMB_REG_HPIN		0x0D
#define SMB_REG_HC3		0x0E
#define SMB_REG_HC4		0x0F
#define SMB_REG_NOTIFY_D0	0x11
#define SMB_REG_NOTIFY_D1	0x12
#define SMB_REG_HPRESCALE1	0x13
#define SMB_REG_HPRESCALE2	0x14
#define SMB_REG_HEXTRA		0x15

#define I2C_TIMEOUT		(10 * USEC_PER_MSEC)
#define USE_DEFAULT		-1

#define I2C_ENC_7BIT_ADDR(x)	(((x) & 0x07F) << 1)
#define I2C_DEC_7BIT_ADDR(x)	(((x) >> 1) & 0x07F)
#define I2C_ENC_10BIT_ADDR(x)	(((x) & 0xFF) | (((x) & 0x0300) << 1) | 0xF000)
#define I2C_DEC_10BIT_ADDR(x)	(((x) & 0xFF) | (((x) >> 1) & 0x300))
#define I2C_IS_10BIT_ADDR(x)	(((x) & 0xF800) == 0xF000)
#define I2C_IS_7BIT_ADDR(x)	(!SUSI_I2C_IS_10BIT_ADDR(x))

#define CHIP_CLK		50000
#define I2C_SCLH_HIGH		2500
#define I2C_SCLH_LOW		1000
#define I2C_SCL_FAST_MODE	0x80
#define I2C_THRESHOLD_SPEED	100
#define I2C_THRESHOLD_SCLH	30
#define I2C_FREQ_MAX		400
#define I2C_FREQ_MIN		8

#define IS_I2C(i2c)			(((i2c)->ch == i2c0) || ((i2c)->ch == i2c1))
#define REG_SW(i2c, reg_i2c, reg_smb)	(IS_I2C(i2c) ? reg_i2c : reg_smb)
#define TRIGGER_READ(i2c, data)		(I2C_READ((i2c), IS_I2C(i2c) ? \
					 I2C_REG_DATA : SMB_REG_HD0, data))
#define I2C_WRITE(i2c, offset, val)	(regmap_write(regmap, \
					 (i2c)->base + (offset), val))
#define I2C_READ(i2c, offset, val)	(regmap_read(regmap, \
					 (i2c)->base + (offset), val))

enum i2c_ch {
	i2c0,
	i2c1,
	smb0,
	smb1,
};

struct dev_i2c {
	u16 base;
	enum i2c_ch ch;
	struct device *dev;
	struct i2c_adapter adap;
	struct rt_mutex lock;
};

/* Pointer to the eiois200_core device structure */
static struct eiois200_dev *eiois200_dev;

static struct regmap *regmap;

static int timeout = I2C_TIMEOUT;
module_param(timeout, int, 0444);
MODULE_PARM_DESC(timeout, "Set IO timeout value.\n");

static int i2c0_freq = USE_DEFAULT;
module_param(i2c0_freq, int, 0444);
MODULE_PARM_DESC(i2c0_freq, "Set EIO-IS200's I2C0 freq.\n");

static int i2c1_freq = USE_DEFAULT;
module_param(i2c1_freq, int, 0444);
MODULE_PARM_DESC(i2c1_freq, "Set EIO-IS200's I2C1 freq.\n");

static int smb0_freq = USE_DEFAULT;
module_param(smb0_freq, int, 0444);
MODULE_PARM_DESC(smb0_freq, "Set EIO-IS200's SMB0 freq.\n");

static int smb1_freq = USE_DEFAULT;
module_param(smb1_freq, int, 0444);
MODULE_PARM_DESC(smb1_freq, "Set EIO-IS200's SMB1 freq.\n");

#if KERNEL_VERSION(5, 12, 0) > LINUX_VERSION_CODE
static void devm_i2c_adapter_release(struct device *dev, void *res)
{
	i2c_del_adapter(*(struct i2c_adapter **)res);
}

static int devm_i2c_add_adapter(struct device *dev, struct i2c_adapter *adap)
{
	struct i2c_adapter **ptr;
	int ret;

	ptr = devres_alloc(devm_i2c_adapter_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	ret = i2c_add_adapter(adap);
	if (ret) {
		devres_free(ptr);
		return ret;
	}

	*ptr = adap;
	devres_add(dev, ptr);

	return ret;
}
#endif

/* my_delay modified from fsleep */
static void my_delay(int cnt)
{
	cnt /= 5;

	if (cnt <= 10)
		udelay(cnt);
	else
		usleep_range(cnt, 2 * cnt);
}

static int reg_or(struct dev_i2c *i2c, int reg, int val_or)
{
	int ret, val = 0;

	ret = I2C_READ(i2c, reg, &val);
	if (ret)
		return ret;

	return I2C_WRITE(i2c, reg, val | val_or);
}

static int reg_and(struct dev_i2c *i2c, int reg, int val_and)
{
	int ret, val = 0;

	ret = I2C_READ(i2c, reg, &val);
	if (ret)
		return ret;

	return I2C_WRITE(i2c, reg, val & val_and);
}

/* Wait busy released */
static int wait_busy(struct dev_i2c *i2c)
{
	ktime_t time_end = ktime_add_us(ktime_get(), timeout);
	int reg = REG_SW(i2c, I2C_REG_STAT, SMB_REG_HS);
	int target = REG_SW(i2c, I2C_STAT_BUSY, SMB_HS_BUSY);
	int val, cnt = 0;

	do {
		my_delay(cnt++);

		if (ktime_after(ktime_get(), time_end)) {
			dev_dbg(i2c->dev, "Wait I2C bus busy timeout\n");
			return -ETIME;
		}

		I2C_READ(i2c, reg, &val);

	} while (val & target);

	return 0;
}

/* Force send 9 clocks to reset bus */
static void reset_bus(struct dev_i2c *i2c)
{
	ktime_t time_end = ktime_add_us(ktime_get(), timeout);
	int val = 0, cnt = 0;

	int reg = REG_SW(i2c, I2C_REG_ECTRL, SMB_REG_HC2);
	int target = REG_SW(i2c, I2C_ECTRL_RST, SMB_HC2_SRESET);

	dev_dbg(i2c->dev, "i2c[%d] bus reset\n", i2c->ch);
	if (IS_I2C(i2c))
		I2C_WRITE(i2c, I2C_REG_ECTRL, I2C_ECTRL_RST);
	else
		reg_or(i2c, SMB_REG_HC2, SMB_HC2_SRESET);

	do {
		my_delay(cnt++);

		if (ktime_after(ktime_get(), time_end)) {
			dev_err(i2c->dev, "bus reset timeout\n");
			return;
		}

		I2C_READ(i2c, reg, &val);
	} while (val & target);

	wait_busy(i2c);
}

static int wait_bus_free(struct dev_i2c *i2c)
{
	ktime_t time_end = ktime_add_us(ktime_get(), timeout);
	int val;
	int cnt = 0;

	/* Wait if I2C channel is resetting */
	do {
		my_delay(cnt);

		if (ktime_after(ktime_get(), time_end)) {
			dev_dbg(i2c->dev, "Wait bus reset timeout\n");
			return -ETIME;
		}

		I2C_READ(i2c, REG_SW(i2c, I2C_REG_ECTRL, SMB_REG_HC2), &val);

	} while (val & REG_SW(i2c, I2C_ECTRL_RST, SMB_HC2_SRESET));

	/* Wait INUSE */
	cnt = 0;
	time_end = ktime_add_us(ktime_get(), timeout);

	do {
		my_delay(cnt);

		if (ktime_after(ktime_get(), time_end)) {
			dev_err(i2c->dev, "I2C bus inuse\n");
			return -ETIME;
		}

		I2C_READ(i2c, REG_SW(i2c, I2C_REG_SEM, SMB_REG_HS), &val);
	} while (val & REG_SW(i2c, I2C_SEM_INUSE, SMB_HS_INUSE));

	return 0;
}

/* Send stop signal after this message */
static int let_stop(struct dev_i2c *i2c)
{
	int reg = REG_SW(i2c, I2C_REG_CTRL, SMB_REG_HC);
	int target = REG_SW(i2c, I2C_CTRL_STOP, SMB_HC_LAST_BYTE);

	return reg_or(i2c, reg, target);
}

static int clr_inuse(struct dev_i2c *i2c)
{
	if (IS_I2C(i2c))
		return I2C_WRITE(i2c, I2C_REG_SEM, I2C_SEM_INUSE);

	return reg_or(i2c, SMB_REG_HS, SMB_HS_INUSE);
}

static int bus_stop(struct dev_i2c *i2c)
{
	ktime_t time_end = ktime_add_us(ktime_get(), timeout);
	int reg = REG_SW(i2c, I2C_REG_CTRL, SMB_REG_HC);
	int target = REG_SW(i2c, I2C_CTRL_STOP, SMB_HC_LAST_BYTE);
	int val = 0, cnt = 0;

	reg_or(i2c, reg, target);

	do {
		my_delay(cnt++);

		if (ktime_after(ktime_get(), time_end)) {
			dev_err(i2c->dev, "wait bus stop complete timeout\n");
			return -ETIME;
		}

		I2C_READ(i2c, reg, &val);
	} while (val & target);

	return 0;
}

static void switch_i2c_mode(struct dev_i2c *i2c, bool on)
{
	u32 tmp;
	
	if (IS_I2C(i2c))
		return;

	I2C_READ(i2c, SMB_REG_HC2, &tmp);
	I2C_WRITE(i2c, SMB_REG_HC2, 
		  on ? tmp |  SMB_HC2_I2C_EN | SMB_HC2_SRESET
		     : tmp & ~SMB_HC2_I2C_EN);
}

static void i2c_clear(struct dev_i2c *i2c)
{
	if (IS_I2C(i2c)) {
		I2C_WRITE(i2c, I2C_REG_STAT, 0xFF);
	} else {
		reg_or(i2c, SMB_REG_HS, 0xA9);
		reg_or(i2c, SMB_REG_HS2, 0x4C);
	}
}

static int wait_write_done(struct dev_i2c *i2c, bool no_ack)
{
	ktime_t time_end = ktime_add_us(ktime_get(), timeout);
	int val, cnt = 0;
	int reg = REG_SW(i2c, I2C_REG_STAT, SMB_REG_HS);
	int target = REG_SW(i2c, I2C_STAT_TXDONE, SMB_HS_TX_DONE);

	do {
		my_delay(cnt++);

		if (ktime_after(ktime_get(), time_end)) {
			if (IS_I2C(i2c)) {
				reg_or(i2c, I2C_REG_STAT, 0);
			} else {
				reg_or(i2c, SMB_REG_HS, 0);
				reg_or(i2c, SMB_REG_HS2, 0);
			}
			dev_err(i2c->dev, "wait write complete timeout %X %X\n", val, target);
			return -ETIME;
		}

		I2C_READ(i2c, reg, &val);

	} while ((val & target) == 0);

	if (no_ack)
		return 0;
	if (IS_I2C(i2c)) {
		reg_or(i2c, I2C_REG_STAT, 0);

		return (val & I2C_STAT_NAK_ERR) ? -EIO : 0;
	}

	reg_or(i2c, SMB_REG_HS, 0);
	I2C_READ(i2c, SMB_REG_HS2, &val);
	I2C_WRITE(i2c, SMB_REG_HS2, val);

	return (val & SMB_HS2_NACK_ERR) ? -EIO : 0;
}

static int wait_ready(struct dev_i2c *i2c)
{
	int ret;

	ret = wait_bus_free(i2c);
	if (ret)
		return ret;

	if (wait_busy(i2c) == 0)
		return 0;

	reset_bus(i2c);

	return wait_busy(i2c);
}

static int write_addr(struct dev_i2c *i2c, int addr, bool no_ack)
{
	I2C_WRITE(i2c, REG_SW(i2c, I2C_REG_ADDR, SMB_REG_HADDR), addr);

	return wait_write_done(i2c, no_ack);
}

static int write_data(struct dev_i2c *i2c, int data, bool no_ack)
{
	I2C_WRITE(i2c, REG_SW(i2c, I2C_REG_DATA, SMB_REG_HD0), data);

	return wait_write_done(i2c, no_ack);
}

static int read_data(struct dev_i2c *i2c, void *data)
{
	int val, cnt = 0;
	ktime_t time_end = ktime_add_us(ktime_get(), timeout);
	int stat = REG_SW(i2c, I2C_REG_STAT, SMB_REG_HS);
	int target = REG_SW(i2c, I2C_STAT_RXREADY, SMB_HS_RX_READY);
	int reg = REG_SW(i2c, I2C_REG_DATA, SMB_REG_HD0);

	do {
		my_delay(cnt++);
		if (ktime_after(ktime_get(), time_end)) {
			reg_or(i2c, stat, 0);

			dev_err(i2c->dev, "read data timeout\n");
			return -ETIME;
		}

		I2C_READ(i2c, stat, &val);

	} while ((val & target) != target);

	/* clear status */
	I2C_WRITE(i2c, stat, val);

	/* Must read data after clear status	*/
	/* or error will occur when high speed. */
	I2C_READ(i2c, reg, data);

	return 0;
}

static int set_freq(struct dev_i2c *i2c, int freq)
{
	u8 pre1, pre2;
	u16 speed;
	int reg1 = IS_I2C(i2c) ? I2C_REG_PRESCALE1 : SMB_REG_HPRESCALE1;
	int reg2 = IS_I2C(i2c) ? I2C_REG_PRESCALE2 : SMB_REG_HPRESCALE2;

	dev_dbg(i2c->dev, "set freq: %dkHz\n", freq);
	if (freq > I2C_FREQ_MAX || freq < I2C_FREQ_MIN) {
		dev_err(i2c->dev, "Invalid i2c freq: %d\n", freq);
		return -EINVAL;
	}

	speed = freq < I2C_THRESHOLD_SCLH ? I2C_SCLH_LOW : I2C_SCLH_HIGH;

	pre1 = (uint8_t)(CHIP_CLK / speed);
	pre2 = (uint8_t)((speed / freq) - 1);

	if (freq > I2C_THRESHOLD_SCLH)
		pre2 |= I2C_SCL_FAST_MODE;

	I2C_WRITE(i2c, reg1, pre1);
	I2C_WRITE(i2c, reg2, pre2);

	return 0;
}

static int get_freq(struct dev_i2c *i2c, int *freq)
{
	int clk;
	int pre1 = 0, pre2 = 0;
	int reg1 = IS_I2C(i2c) ? I2C_REG_PRESCALE1 : SMB_REG_HPRESCALE1;
	int reg2 = IS_I2C(i2c) ? I2C_REG_PRESCALE2 : SMB_REG_HPRESCALE2;

	I2C_READ(i2c, reg1, &pre1);
	I2C_READ(i2c, reg2, &pre2);

	clk = pre2 & I2C_SCL_FAST_MODE ? I2C_SCLH_HIGH : I2C_SCLH_LOW;
	pre2 &= ~I2C_SCL_FAST_MODE;

	*freq = clk / (pre2 + 1);

	return 0;
}

static int smb_access(struct dev_i2c *i2c, u8 addr, bool is_read, u8 cmd,
		      int size, union i2c_smbus_data *data)
{
	int i, tmp, ret = 0;
	int st1, st2;
	struct device *dev = i2c->dev;
	int len = 0;

	rt_mutex_lock(&i2c->lock);

	ret = wait_ready(i2c);
	if (ret)
		goto exit;
	
	switch_i2c_mode(i2c, false);
	addr = I2C_ENC_7BIT_ADDR(addr) | is_read;
	I2C_WRITE(i2c, SMB_REG_HADDR, addr);
	I2C_WRITE(i2c, SMB_REG_HCMD, cmd);
	dev_dbg(dev, "SMB[%d], addr:0x%02X, cmd:0x%02X size=%d\n", i2c->ch, addr, cmd, size);

	switch (size) {
	case I2C_SMBUS_QUICK:
		dev_dbg(dev, "I2C_SMBUS_QUICK\n");
		break;
	case I2C_SMBUS_BYTE:
		if (!is_read) {
			dev_dbg(dev, "I2C_SMBUS_BYTE\n");
			I2C_WRITE(i2c, SMB_REG_HCMD, cmd);
		}
		break;
	case I2C_SMBUS_BYTE_DATA:
		dev_dbg(dev, "I2C_SMBUS_BYTE_DATA\n");
		if (!is_read) {
			I2C_WRITE(i2c, SMB_REG_HD0, data->byte);
			dev_dbg(dev, "write %X\n", data->byte);
		}
		break;
	case I2C_SMBUS_WORD_DATA:
		dev_dbg(dev, "I2C_SMBUS_WORD_DATA\n");
		if (!is_read) {
			I2C_WRITE(i2c, SMB_REG_HD0, data->block[0]);
			I2C_WRITE(i2c, SMB_REG_HD1, data->block[1]);
		}
		break;
	case I2C_SMBUS_PROC_CALL:
		dev_dbg(dev, "I2C_SMBUS_PROC_CALL\n");
		I2C_WRITE(i2c, SMB_REG_HD0, data->block[0]);
		I2C_WRITE(i2c, SMB_REG_HD1, data->block[1]);
		break;
	case I2C_SMBUS_BLOCK_DATA:
		dev_dbg(dev, "I2C_SMBUS_BLOCK_DATA\n");
		// BLOCK need to set cmd type first.
		I2C_READ(i2c, SMB_REG_HC, &tmp);
		tmp &= ~(0x07 << SMB_HC_CMD_SHIFT);
		tmp |= (size << SMB_HC_CMD_SHIFT);
		I2C_WRITE(i2c, SMB_REG_HC, tmp);
		I2C_WRITE(i2c, SMB_REG_HADDR, addr & ~0x01);

		// Reset internal buffer index pointer
		reg_and(i2c, SMB_REG_HC2, (int)~SMB_HC2_E32B);
		reg_or(i2c, SMB_REG_HC2, (int)SMB_HC2_E32B);

		if (!is_read) {
			I2C_WRITE(i2c, SMB_REG_HD0, data->block[0]);
			for (i = 1; i <= data->block[0]; i++)
				I2C_WRITE(i2c, SMB_REG_HBLOCK, data->block[i]);
		}
		break;
	case I2C_SMBUS_BLOCK_PROC_CALL:
		reg_and(i2c, SMB_REG_HC, 0x07 << SMB_HC_CMD_SHIFT);
		I2C_WRITE(i2c, SMB_REG_HD0, data->block[0]);

		// Reset internal buffer index pointer
		reg_and(i2c, SMB_REG_HC2, (int)~SMB_HC2_E32B);
		reg_or(i2c, SMB_REG_HC2, (int)SMB_HC2_E32B);

		for (i = 1; i <= data->block[0]; i++)
			I2C_WRITE(i2c, SMB_REG_HBLOCK, data->block[i]);
		break;
	default:
		ret = -EINVAL;
		goto exit;
	}

	I2C_READ(i2c, SMB_REG_HC, &tmp);
	tmp &= ~(0x07 << SMB_HC_CMD_SHIFT);
	tmp |= (size << SMB_HC_CMD_SHIFT) | SMB_HC_START;
	tmp &= ~(SMB_HC_I2C_NACKEN | SMB_HC_KILL | SMB_HC_PEC_EN);
	I2C_WRITE(i2c, SMB_REG_HC, tmp);

	ret = wait_busy(i2c);
	if (ret)
		goto exit;

	I2C_READ(i2c, SMB_REG_HS, &st1);
	I2C_READ(i2c, SMB_REG_HS2, &st2);

	if (st1 & SMB_HS_FAILED) {
		dev_dbg(dev, "HS FAILED\n");
		ret = -EIO;
	} else if (st1 & SMB_HS_ARL_ERR) {
		dev_dbg(dev, "ARL FAILED\n");
		ret = -EIO;
	} else if (st2 & SMB_HS2_TO_ERR) {
		dev_dbg(dev, "timeout\n");
		ret = -ETIME;
	} else if (st2 & SMB_HS2_NACK_ERR) {
		dev_dbg(dev, "NACK err\n");
		ret = -EIO;
	} else if (st2 & SMB_HS2_PEC_ERR) {
		dev_dbg(dev, "PEC err\n");
		ret = -EIO;
	}
	if (ret)
		goto exit;

	switch (size) {
	case I2C_SMBUS_QUICK:
		dev_dbg(dev, "I2C_SMBUS_QUICK\n");
		break;
	case I2C_SMBUS_BYTE:
	case I2C_SMBUS_BYTE_DATA:
		if (is_read) {
			dev_dbg(dev, "I2C_SMBUS_BYTE/I2C_SMBUS_BYTE_DATA\n");
			I2C_READ(i2c, SMB_REG_HD0, (u32 *)data->block);
			dev_dbg(dev, "read %X\n", data->block[0]);
		}
		break;
	case I2C_SMBUS_WORD_DATA:
		if (is_read) {
			dev_dbg(dev, "I2C_SMBUS_WORD_DATA\n");
			I2C_READ(i2c, SMB_REG_HD0, (u32 *)data->block + 0);
			I2C_READ(i2c, SMB_REG_HD1, (u32 *)data->block + 1);
		}
		break;
	case I2C_SMBUS_PROC_CALL:
		dev_dbg(dev, "I2C_SMBUS_PROC_CALL\n");
		I2C_READ(i2c, SMB_REG_HD0, (u32 *)data->block + 0);
		I2C_READ(i2c, SMB_REG_HD1, (u32 *)data->block + 1);
		break;
	case I2C_SMBUS_BLOCK_DATA:
		if (!is_read) 
			break;

		dev_dbg(dev, "I2C_SMBUS_BLOCK_DATA\n");			
		I2C_READ(i2c, SMB_REG_HD0, &len);
		len = min(len, I2C_SMBUS_BLOCK_MAX);
		data->block[0] = len;

		for (i = 1; i < len; i++)
			I2C_READ(i2c, SMB_REG_HBLOCK, (void *)data->block + i);
		break;
	default:
		ret = -EINVAL;
		goto exit;
	}

exit:
	I2C_WRITE(i2c, SMB_REG_HS, 0xFF);
	I2C_WRITE(i2c, SMB_REG_HS2, 0xFF);

	rt_mutex_unlock(&i2c->lock);
	return ret;
}

static int i2c_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	int msg, data;
	int addr = 0;
	int dummy;
	int ret = 0;
	struct dev_i2c *i2c = i2c_get_adapdata(adap);

	rt_mutex_lock(&i2c->lock);

	ret = wait_ready(i2c);
	if (ret)
		goto exit;
	
	switch_i2c_mode(i2c, true);

	dev_dbg(i2c->dev, "Transmit %d I2C messages\n", num);
	for (msg = 0; msg < num; msg++)	{
		int is_read = msgs[msg].flags & I2C_M_RD;
		bool no_ack = msgs[msg].flags & I2C_M_IGNORE_NAK;

		dev_dbg(i2c->dev, "messages %d len= %d\n", msg, msgs[msg].len);
		if (!msgs[msg].len)
			let_stop(i2c);

		if (msgs[msg].flags & I2C_M_TEN) {
			dev_dbg(i2c->dev, "10bits addr: %X\n", addr);
			addr = I2C_ENC_10BIT_ADDR(msgs[msg].addr);
			addr |= is_read;

			ret = write_addr(i2c, addr >> 8, no_ack);
			if (!ret)
				ret = write_data(i2c, addr & 0x7F,
						 no_ack);
		} else {
			dev_dbg(i2c->dev, "7bits addr: %X\n", addr);
			addr = I2C_ENC_7BIT_ADDR(msgs[msg].addr);
			addr |= is_read;

			ret = write_addr(i2c, addr, no_ack);
		}

		if (ret != 0)
			goto exit;

		if (!msgs[msg].len)
			goto exit;

		if (is_read)
			ret = TRIGGER_READ(i2c, &dummy);

		/* Transmit all messages */
		for (data = 0; data < msgs[msg].len; data++) {
			if (msgs[msg].flags & I2C_M_RD) {
				bool last = msgs[msg].len == data + 1;

				if (last)
					let_stop(i2c);

				ret = read_data(i2c, &msgs[msg].buf[data]);
				dev_dbg(i2c->dev, "I2C read[%d] = %x\n",
					data, msgs[msg].buf[data]);

				/* Don't stop twice */
				if (last && ret == 0)
					goto exit;
			} else {
				ret = write_data(i2c, msgs[msg].buf[data],
						 no_ack);
				dev_dbg(i2c->dev, "I2C write[%d] = %x\n",
					data, msgs[msg].buf[data]);
			}
			if (ret != 0)
				goto exit;
		}
	}

	if (!ret)
		ret = bus_stop(i2c);

	if (!ret)
		goto exit;

exit:
	if (ret)
		reset_bus(i2c);

	i2c_clear(i2c);
	clr_inuse(i2c);

	rt_mutex_unlock(&i2c->lock);
	return ret ? ret : num;
}

static int smbus_xfer(struct i2c_adapter *adap, u16 addr,
		      u16 flags, char is_read, u8 cmd,
		      int size, union i2c_smbus_data *data)
{
	int ret;
	struct dev_i2c *i2c = i2c_get_adapdata(adap);
	int num = is_read ? 2 : 1;
	struct device *dev = i2c->dev;
	u8 buf[I2C_SMBUS_BLOCK_MAX + sizeof(u32)] = { cmd, };
	struct i2c_msg msgs[2] = {
		{ .addr = addr, .flags = flags & ~I2C_M_RD, .buf = buf + 0},
		{ .addr = addr, .flags = flags | I2C_M_RD, .buf = buf + 1},
	};

	if (!IS_I2C(i2c) && size != I2C_SMBUS_I2C_BLOCK_DATA)
		return smb_access(i2c, addr, is_read, cmd, size, data);

	if (data) {
		buf[0] = cmd;
		msgs[0].flags = is_read ? flags : flags & ~I2C_M_RD;
		msgs[1].buf = data->block;
	}

	switch (size) {
	case I2C_SMBUS_QUICK:
		dev_dbg(dev, "I2C_SMBUS_QUICK on I2C\n");
		num = 1;
		break;
	case I2C_SMBUS_BYTE:
		dev_dbg(dev, "I2C_SMBUS_BYTE on I2C\n");
		num = 1;
		msgs[0].len = 1;
		msgs[0].buf = is_read ? data->block : buf;
		msgs[0].flags = is_read ? flags | I2C_M_RD :
					  flags & I2C_M_RD;
		break;
	case I2C_SMBUS_BYTE_DATA:
		dev_dbg(dev, "I2C_SMBUS_BYTE_DATA on I2C\n");
		msgs[0].len = is_read ? 1 : 2;
		buf[1] = data->block[0];
		msgs[1].len = 1;
		break;
	case I2C_SMBUS_WORD_DATA:
		dev_dbg(dev, "I2C_SMBUS_WORD_DATA on I2C\n");
		msgs[0].len = is_read ? 1 : 3;
		msgs[1].len = 2;
		buf[1] = data->block[0];
		buf[2] = data->block[1];
		msgs[1].buf = data->block;
		break;
	case I2C_SMBUS_I2C_BLOCK_DATA:
		dev_dbg(dev, "I2C_SMBUS_I2C_BLOCK_DATA on I2C len=%d\n",
			data->block[0]);
		msgs[0].len = is_read ? 1 : data->block[0] + 1;
		msgs[1].len = data->block[0];
		msgs[1].buf = data->block + 1;
		if (msgs[0].len >= I2C_SMBUS_BLOCK_MAX ||
		    msgs[1].len >= I2C_SMBUS_BLOCK_MAX)
			return -EINVAL;
		if (!is_read)
			memcpy(buf + 1, data->block + 1, msgs[0].len);
		break;
	case I2C_SMBUS_PROC_CALL:
		dev_dbg(dev, "I2C_SMBUS_PROC_CALL on I2C\n");
		num = 2;
		msgs[0].flags = flags & ~I2C_M_RD;
		msgs[0].len = 3;
		buf[1] = data->block[0];
		buf[2] = data->block[1];
		msgs[1].len = 2;
		break;
	case I2C_SMBUS_BLOCK_DATA:
		dev_dbg(dev, "I2C_SMBUS_BLOCK_DATA on I2C not supported\n");
		return -EINVAL;
	case I2C_SMBUS_I2C_BLOCK_BROKEN:
		dev_dbg(dev, "I2C_SMBUS_I2C_BLOCK_BROKEN on I2C not supported\n");
		return -EINVAL;
	case I2C_SMBUS_BLOCK_PROC_CALL:
		dev_dbg(dev, "I2C_SMBUS_BLOCK_PROC_CALL on I2C not supported\n");
		return -EINVAL;
	default:
		return -EINVAL;
	}

	ret = i2c_xfer(adap, msgs, num);
	return ret < 0 ? ret : 0;
}

static int load_i2c(struct device *dev, enum i2c_ch ch, struct dev_i2c *i2c)
{
	u32 base_lo, base_hi, base;
	int ldn = LDN_I2C0 + ch;
	int *freqs[] = { &i2c0_freq, &i2c1_freq, &smb0_freq, &smb1_freq };
	int *freq = freqs[ch];

	mutex_lock(&eiois200_dev->mutex);

	/* Get device I/O base address */
	if (regmap_write(regmap, REG_PNP_INDEX, REG_EXT_MODE_ENTER) ||
	    regmap_write(regmap, REG_PNP_INDEX, REG_EXT_MODE_ENTER) ||
	    regmap_write(regmap, REG_PNP_INDEX, REG_LDN) ||
	    regmap_write(regmap, REG_PNP_DATA, ldn) ||
	    regmap_write(regmap, REG_PNP_INDEX, REG_BASE_HI) ||
	    regmap_read(regmap, REG_PNP_DATA, &base_hi) ||
	    regmap_write(regmap, REG_PNP_INDEX, REG_BASE_LO) ||
	    regmap_read(regmap, REG_PNP_DATA, &base_lo) ||
	    regmap_write(regmap, REG_PNP_INDEX, REG_EXT_MODE_EXIT)) {
		mutex_unlock(&eiois200_dev->mutex);

		dev_err(dev, "error read/write I2C[%d] IO port\n", ch);
		return -EIO;
	}

	mutex_unlock(&eiois200_dev->mutex);

	base = (base_hi << 8) | base_lo;
	if (base == 0xFFFF || base == 0) {
		dev_dbg(dev, "i2c[%d] base addr= %XH --> not inuse\n",
			ch, base);
		return -EINVAL;
	}

	dev_dbg(dev, "i2c[%d] base addr= %XH\n", ch, base);
	i2c->base = base;
	i2c->ch = ch;
	i2c->dev = dev;

	if (*freq != USE_DEFAULT)
		set_freq(i2c, *freq);

	get_freq(i2c, freq);

	return 0;
}

static u32 functionality(struct i2c_adapter *adapter)
{
	struct dev_i2c *i2c = i2c_get_adapdata(adapter);

	return REG_SW(i2c, SUPPORTED_I2C, SUPPORTED_SMB);
}

static const struct i2c_algorithm algo = {
	.smbus_xfer	= smbus_xfer,
	.master_xfer	= i2c_xfer,
	.functionality	= functionality,
};

static int probe(struct platform_device *pdev)
{
	static const char * const name[] = { "i2c0", "i2c1", "smb0", "smb1" };
	int ret = 0;
	enum i2c_ch ch;
	struct device *dev = &pdev->dev;

	if ((timeout < I2C_TIMEOUT / 100) || (timeout > I2C_TIMEOUT * 100)) {
		dev_err(dev, "Error timeout value %d\n", timeout);
		return -EINVAL;
	}

	eiois200_dev = dev_get_drvdata(dev->parent);
	if (!eiois200_dev) {
		dev_err(dev, "Error contact eiois200_core %d\n", ret);
		return -ENXIO;
	}

	regmap = dev_get_regmap(dev->parent, NULL);
	if (!regmap) {
		dev_err(dev, "Query parent regmap fail\n");
		return -ENOMEM;
	}

	for (ch = i2c0; ch < MAX_I2C_SMB; ch++) {
		struct dev_i2c *i2c;

		i2c = devm_kzalloc(dev, sizeof(*i2c), GFP_KERNEL);
		if (!i2c)
			return -ENOMEM;

		if (load_i2c(dev, ch, i2c))
			continue;

		i2c->adap.owner = THIS_MODULE;
		i2c->adap.class = I2C_CLASS_HWMON | I2C_CLASS_SPD;
		i2c->adap.algo = &algo;
		i2c->adap.dev.parent = dev;
		rt_mutex_init(&i2c->lock);

		sprintf(i2c->adap.name, "eiois200-%s", name[ch]);
		i2c_set_adapdata(&i2c->adap, i2c);

		ret = devm_i2c_add_adapter(dev, &i2c->adap);
		dev_dbg(dev, "Add I2C_SMB[%d] %s. ret=%d\n",
			ch, ret ? "Error" : "Success", ret);
		if (ret)
			return ret;
	}

	return 0;
}

static struct platform_driver i2c_driver = {
	.driver.name = "i2c_eiois200",
};

module_platform_driver_probe(i2c_driver, probe);

MODULE_AUTHOR("Adavantech");
MODULE_DESCRIPTION("I2C driver for Advantech EIO-IS200 embedded controller");
MODULE_LICENSE("GPL v2");
