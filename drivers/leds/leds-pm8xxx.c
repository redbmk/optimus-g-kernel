/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#define Ledlog(x, ...) printk(x, ##__VA_ARGS__)

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/workqueue.h>
#include <linux/err.h>
#include <linux/ctype.h>

#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/pwm.h>
#include <linux/leds-pm8xxx.h>

#define SSBI_REG_ADDR_DRV_KEYPAD	0x48
#define PM8XXX_DRV_KEYPAD_BL_MASK	0xf0
#define PM8XXX_DRV_KEYPAD_BL_SHIFT	0x04

#define SSBI_REG_ADDR_FLASH_DRV0        0x49
#define PM8XXX_DRV_FLASH_MASK           0xf0
#define PM8XXX_DRV_FLASH_SHIFT          0x04

#define SSBI_REG_ADDR_FLASH_DRV1        0xFB

#define SSBI_REG_ADDR_LED_CTRL_BASE	0x131
#define SSBI_REG_ADDR_LED_CTRL(n)	(SSBI_REG_ADDR_LED_CTRL_BASE + (n))
#define PM8XXX_DRV_LED_CTRL_MASK	0xf8
#define PM8XXX_DRV_LED_CTRL_SHIFT	0x03

#define SSBI_REG_ADDR_WLED_CTRL_BASE	0x25A
#define SSBI_REG_ADDR_WLED_CTRL(n)	(SSBI_REG_ADDR_WLED_CTRL_BASE + (n) - 1)

/* wled control registers */
#define WLED_MOD_CTRL_REG		SSBI_REG_ADDR_WLED_CTRL(1)
#define WLED_MAX_CURR_CFG_REG(n)	SSBI_REG_ADDR_WLED_CTRL(n + 2)
#define WLED_BRIGHTNESS_CNTL_REG1(n)	SSBI_REG_ADDR_WLED_CTRL(n + 5)
#define WLED_BRIGHTNESS_CNTL_REG2(n)	SSBI_REG_ADDR_WLED_CTRL(n + 6)
#define WLED_SYNC_REG			SSBI_REG_ADDR_WLED_CTRL(11)
#define WLED_OVP_CFG_REG		SSBI_REG_ADDR_WLED_CTRL(13)
#define WLED_BOOST_CFG_REG		SSBI_REG_ADDR_WLED_CTRL(14)
#define WLED_HIGH_POLE_CAP_REG		SSBI_REG_ADDR_WLED_CTRL(16)

#define WLED_STRINGS			0x03
#define WLED_OVP_VAL_MASK		0x30
#define WLED_OVP_VAL_BIT_SHFT		0x04
#define WLED_BOOST_LIMIT_MASK		0xE0
#define WLED_BOOST_LIMIT_BIT_SHFT	0x05
#define WLED_BOOST_OFF			0x00
#define WLED_EN_MASK			0x01
#define WLED_CP_SELECT_MAX		0x03
#define WLED_CP_SELECT_MASK		0x03
#define WLED_DIG_MOD_GEN_MASK		0x70
#define WLED_CS_OUT_MASK		0x0E
#define WLED_CTL_DLY_STEP		200
#define WLED_CTL_DLY_MAX		1400
#define WLED_CTL_DLY_MASK		0xE0
#define WLED_CTL_DLY_BIT_SHFT		0x05
#define WLED_MAX_CURR			25
#define WLED_MAX_CURR_MASK		0x1F
#define WLED_OP_FDBCK_MASK		0x1C
#define WLED_OP_FDBCK_BIT_SHFT		0x02

#define WLED_MAX_LEVEL			255
#define WLED_8_BIT_MASK			0xFF
#define WLED_8_BIT_SHFT			0x08
#define WLED_MAX_DUTY_CYCLE		0xFFF

#define WLED_SYNC_VAL			0x07
#define WLED_SYNC_RESET_VAL		0x00

#define SSBI_REG_ADDR_RGB_CNTL1		0x12D
#define SSBI_REG_ADDR_RGB_CNTL2		0x12E

#define PM8XXX_DRV_RGB_RED_LED		BIT(2)
#define PM8XXX_DRV_RGB_GREEN_LED	BIT(1)
#define PM8XXX_DRV_RGB_BLUE_LED		BIT(0)

#define MAX_FLASH_LED_CURRENT		300
#define MAX_LC_LED_CURRENT		40
#define MAX_KP_BL_LED_CURRENT		300

#define PM8XXX_ID_LED_CURRENT_FACTOR	2  /* Iout = x * 2mA */
#define PM8XXX_ID_FLASH_CURRENT_FACTOR	20 /* Iout = x * 20mA */

#define PM8XXX_FLASH_MODE_DBUS1		1
#define PM8XXX_FLASH_MODE_DBUS2		2
#define PM8XXX_FLASH_MODE_PWM		3

#define MAX_LC_LED_BRIGHTNESS		20
#define MAX_FLASH_BRIGHTNESS		15
#define MAX_KB_LED_BRIGHTNESS		15

#define PM8XXX_LED_OFFSET(id) ((id) - PM8XXX_ID_LED_0)

#define PM8XXX_LED_PWM_FLAGS	(PM_PWM_LUT_LOOP | PM_PWM_LUT_RAMP_UP | \
				PM_PWM_LUT_PAUSE_LO_EN | PM_PWM_LUT_PAUSE_HI_EN)

#define PM8XXX_LED_PWM_GRPFREQ_MAX	255
#define PM8XXX_LED_PWM_GRPPWM_MAX	255

#define LED_MAP(_version, _kb, _led0, _led1, _led2, _flash_led0, _flash_led1, \
	_wled, _rgb_led_red, _rgb_led_green, _rgb_led_blue)\
	{\
		.version = _version,\
		.supported = _kb << PM8XXX_ID_LED_KB_LIGHT | \
			_led0 << PM8XXX_ID_LED_0 | _led1 << PM8XXX_ID_LED_1 | \
			_led2 << PM8XXX_ID_LED_2  | \
			_flash_led0 << PM8XXX_ID_FLASH_LED_0 | \
			_flash_led1 << PM8XXX_ID_FLASH_LED_1 | \
			_wled << PM8XXX_ID_WLED | \
			_rgb_led_red << PM8XXX_ID_RGB_LED_RED | \
			_rgb_led_green << PM8XXX_ID_RGB_LED_GREEN | \
			_rgb_led_blue << PM8XXX_ID_RGB_LED_BLUE, \
	}

/**
 * supported_leds - leds supported for each PMIC version
 * @version - version of PMIC
 * @supported - which leds are supported on version
 */

struct supported_leds {
	enum pm8xxx_version version;
	u32 supported;
};

static const struct supported_leds led_map[] = {
	LED_MAP(PM8XXX_VERSION_8058, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0),
	LED_MAP(PM8XXX_VERSION_8921, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0),
	LED_MAP(PM8XXX_VERSION_8018, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0),
	LED_MAP(PM8XXX_VERSION_8922, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1),
	LED_MAP(PM8XXX_VERSION_8038, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1),
};

/**
 * struct pm8xxx_led_data - internal led data structure
 * @led_classdev - led class device
 * @id - led index
 * @work - workqueue for led
 * @lock - to protect the transactions
 * @reg - cached value of led register
 * @pwm_dev - pointer to PWM device if LED is driven using PWM
 * @pwm_channel - PWM channel ID
 * @pwm_period_us - PWM period in micro seconds
 * @pwm_duty_cycles - struct that describes PWM duty cycles info
 * @use_pwm - controlled by userspace
 */
struct pm8xxx_led_data {
	struct led_classdev	cdev;
	int			id;
	u8			reg;
	u8			wled_mod_ctrl_val;
	u8			lock_update;
	u8			blink;
	struct device		*dev;
	struct work_struct	work;
	struct mutex		lock;
	struct pwm_device	*pwm_dev;
	int			pwm_channel;
	u32			pwm_period_us;
	struct pm8xxx_pwm_duty_cycles *pwm_duty_cycles;
	struct wled_config_data *wled_cfg;
	int			max_current;
	int			use_pwm;
	int			adjust_brightness;
	u16			pwm_grppwm;
	u16			pwm_grpfreq;
	u16			pwm_pause_hi;
	u16			pwm_pause_lo;
};

static void led_kp_set(struct pm8xxx_led_data *led, enum led_brightness value)
{
	int rc;
	u8 level;

	level = (value << PM8XXX_DRV_KEYPAD_BL_SHIFT) &
				 PM8XXX_DRV_KEYPAD_BL_MASK;

	led->reg &= ~PM8XXX_DRV_KEYPAD_BL_MASK;
	led->reg |= level;

	rc = pm8xxx_writeb(led->dev->parent, SSBI_REG_ADDR_DRV_KEYPAD,
								led->reg);
	if (rc < 0)
		dev_err(led->cdev.dev,
			"can't set keypad backlight level rc=%d\n", rc);
}

static void led_lc_set(struct pm8xxx_led_data *led, enum led_brightness value)
{
	int rc, offset;
	u8 level;

	level = (value << PM8XXX_DRV_LED_CTRL_SHIFT) &
				PM8XXX_DRV_LED_CTRL_MASK;

	offset = PM8XXX_LED_OFFSET(led->id);

	led->reg &= ~PM8XXX_DRV_LED_CTRL_MASK;
	led->reg |= level;

	rc = pm8xxx_writeb(led->dev->parent, SSBI_REG_ADDR_LED_CTRL(offset),
								led->reg);
	if (rc)
		dev_err(led->cdev.dev, "can't set (%d) led value rc=%d\n",
				led->id, rc);
}

static void
led_flash_set(struct pm8xxx_led_data *led, enum led_brightness value)
{
	int rc;
	u8 level;
	u16 reg_addr;

	level = (value << PM8XXX_DRV_FLASH_SHIFT) &
				 PM8XXX_DRV_FLASH_MASK;

	led->reg &= ~PM8XXX_DRV_FLASH_MASK;
	led->reg |= level;

	if (led->id == PM8XXX_ID_FLASH_LED_0)
		reg_addr = SSBI_REG_ADDR_FLASH_DRV0;
	else
		reg_addr = SSBI_REG_ADDR_FLASH_DRV1;

	rc = pm8xxx_writeb(led->dev->parent, reg_addr, led->reg);
	if (rc < 0)
		dev_err(led->cdev.dev, "can't set flash led%d level rc=%d\n",
			 led->id, rc);
}

static int
led_wled_set(struct pm8xxx_led_data *led, enum led_brightness value)
{
	int rc, duty;
	u8 val, i, num_wled_strings;

	if (value > WLED_MAX_LEVEL)
		value = WLED_MAX_LEVEL;

	if (value == 0) {
		rc = pm8xxx_writeb(led->dev->parent, WLED_MOD_CTRL_REG,
				WLED_BOOST_OFF);
		if (rc) {
			dev_err(led->dev->parent, "can't write wled ctrl config"
				" register rc=%d\n", rc);
			return rc;
		}
	} else {
		rc = pm8xxx_writeb(led->dev->parent, WLED_MOD_CTRL_REG,
				led->wled_mod_ctrl_val);
		if (rc) {
			dev_err(led->dev->parent, "can't write wled ctrl config"
				" register rc=%d\n", rc);
			return rc;
		}
	}

	duty = (WLED_MAX_DUTY_CYCLE * value) / WLED_MAX_LEVEL;

	num_wled_strings = led->wled_cfg->num_strings;

	/* program brightness control registers */
	for (i = 0; i < num_wled_strings; i++) {
		rc = pm8xxx_readb(led->dev->parent,
				WLED_BRIGHTNESS_CNTL_REG1(i), &val);
		if (rc) {
			dev_err(led->dev->parent, "can't read wled brightnes ctrl"
				" register1 rc=%d\n", rc);
			return rc;
		}

		val = (val & ~WLED_MAX_CURR_MASK) | (duty >> WLED_8_BIT_SHFT);
		rc = pm8xxx_writeb(led->dev->parent,
				WLED_BRIGHTNESS_CNTL_REG1(i), val);
		if (rc) {
			dev_err(led->dev->parent, "can't write wled brightness ctrl"
				" register1 rc=%d\n", rc);
			return rc;
		}

		val = duty & WLED_8_BIT_MASK;
		rc = pm8xxx_writeb(led->dev->parent,
				WLED_BRIGHTNESS_CNTL_REG2(i), val);
		if (rc) {
			dev_err(led->dev->parent, "can't write wled brightness ctrl"
				" register2 rc=%d\n", rc);
			return rc;
		}
	}

	/* sync */
	val = WLED_SYNC_VAL;
	rc = pm8xxx_writeb(led->dev->parent, WLED_SYNC_REG, val);
	if (rc) {
		dev_err(led->dev->parent,
			"can't read wled sync register rc=%d\n", rc);
		return rc;
	}

	val = WLED_SYNC_RESET_VAL;
	rc = pm8xxx_writeb(led->dev->parent, WLED_SYNC_REG, val);
	if (rc) {
		dev_err(led->dev->parent,
			"can't read wled sync register rc=%d\n", rc);
		return rc;
	}
	return 0;
}

static void wled_dump_regs(struct pm8xxx_led_data *led)
{
	int i;
	u8 val;

	for (i = 1; i < 17; i++) {
		pm8xxx_readb(led->dev->parent,
				SSBI_REG_ADDR_WLED_CTRL(i), &val);
		pr_debug("WLED_CTRL_%d = 0x%x\n", i, val);
	}
}

static void
led_rgb_write(struct pm8xxx_led_data *led, u16 addr, enum led_brightness value)
{
	int rc;
	u8 val, mask;

	if (led->id != PM8XXX_ID_RGB_LED_BLUE &&
		led->id != PM8XXX_ID_RGB_LED_RED &&
		led->id != PM8XXX_ID_RGB_LED_GREEN)
		return;

	rc = pm8xxx_readb(led->dev->parent, addr, &val);
	if (rc) {
		dev_err(led->cdev.dev, "can't read rgb ctrl register rc=%d\n",
							rc);
		return;
	}

	switch (led->id) {
	case PM8XXX_ID_RGB_LED_RED:
		mask = PM8XXX_DRV_RGB_RED_LED;
		break;
	case PM8XXX_ID_RGB_LED_GREEN:
		mask = PM8XXX_DRV_RGB_GREEN_LED;
		break;
	case PM8XXX_ID_RGB_LED_BLUE:
		mask = PM8XXX_DRV_RGB_BLUE_LED;
		break;
	default:
		return;
	}

	if (value)
		val |= mask;
	else
		val &= ~mask;

	rc = pm8xxx_writeb(led->dev->parent, addr, val);
	if (rc < 0)
		dev_err(led->cdev.dev, "can't set rgb led %d level rc=%d\n",
			 led->id, rc);
}

static void
led_rgb_set(struct pm8xxx_led_data *led, enum led_brightness value)
{
	if (value) {
		led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, value);
		led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL2, value);
	} else {
		led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL2, value);
		led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, value);
	}
}

static int pm8xxx_adjust_brightness(struct led_classdev *led_cdev,
		enum led_brightness value)
{
	int level = 0;
	struct	pm8xxx_led_data *led;
	led = container_of(led_cdev, struct pm8xxx_led_data, cdev);

	if (!led->adjust_brightness)
		return value;

	if (led->adjust_brightness == led->cdev.max_brightness)
		return value;

	level = (2 * value * led->adjust_brightness
			+ led->cdev.max_brightness)
		/ (2 * led->cdev.max_brightness);

	if (!level && value)
		level = 1;

	return level;
}

static int pm8xxx_led_pwm_pattern_update(struct pm8xxx_led_data * led)
{
	int start_idx;

#ifdef LGE_PWM_LED
	int idx_len0, idx_len1;
	int *pcts0 = NULL;
	int *pcts1 = NULL;
	int mid0, mid1;
#else
	int idx_len;
	int *pcts = NULL;
	int mid;
#endif
        
	int i, rc = 0;
	int temp = 0;
	int pwm_max = 0;
	int total_ms, on_ms;

#ifdef LGE_PWM_LED
	if (!led->pwm_duty_cycles ||
			!led->pwm_duty_cycles->duty_pcts0 ||
			!led->pwm_duty_cycles->duty_pcts1) {
		dev_err(led->cdev.dev, "duty_cycles and duty_pcts do not exist\n");
		return -EINVAL;
	}

	if (led->pwm_grppwm > 0 && led->pwm_grpfreq > 0) {
		total_ms = led->pwm_grpfreq * 50;
		on_ms = (led->pwm_grppwm * total_ms) >> 8;
		if (PM8XXX_LED_PWM_FLAGS & PM_PWM_LUT_REVERSE) {
			led->pwm_duty_cycles->duty_ms0 = on_ms /
				(led->pwm_duty_cycles->num_duty_pcts0 << 1);
			led->pwm_duty_cycles->duty_ms1 = on_ms /
				(led->pwm_duty_cycles->num_duty_pcts1 << 1);
			led->pwm_pause_lo = min(
				on_ms % (led->pwm_duty_cycles->num_duty_pcts0 << 1),
				on_ms % (led->pwm_duty_cycles->num_duty_pcts1 << 1));
		} else {
			led->pwm_duty_cycles->duty_ms0 = on_ms /
				(led->pwm_duty_cycles->num_duty_pcts0);
			led->pwm_duty_cycles->duty_ms1 = on_ms /
				(led->pwm_duty_cycles->num_duty_pcts1);
			led->pwm_pause_lo = min(
				on_ms % (led->pwm_duty_cycles->num_duty_pcts0),
				on_ms % (led->pwm_duty_cycles->num_duty_pcts1));
		}
		led->pwm_pause_hi = total_ms - on_ms;
		dev_dbg(led->cdev.dev, "duty_ms0 %d, duty_ms1 %d, pause_hi %d, pause_lo %d, total_ms %d, on_ms %d\n",
				led->pwm_duty_cycles->duty_ms0, led->pwm_duty_cycles->duty_ms1,
				led->pwm_pause_hi, led->pwm_pause_lo, total_ms, on_ms);
	}

	pwm_max = pm8xxx_adjust_brightness(&led->cdev, led->cdev.brightness);
	start_idx = led->pwm_duty_cycles->start_idx;
#else
	if (!led->pwm_duty_cycles || !led->pwm_duty_cycles->duty_pcts) {
		dev_err(led->cdev.dev, "duty_cycles and duty_pcts is not exist\n");
		return -EINVAL;
	}

	if (led->pwm_grppwm > 0 && led->pwm_grpfreq > 0) {
		total_ms = led->pwm_grpfreq * 50;
		on_ms = (led->pwm_grppwm * total_ms) >> 8;
		if (PM8XXX_LED_PWM_FLAGS & PM_PWM_LUT_REVERSE) {
			led->pwm_duty_cycles->duty_ms = on_ms /
				(led->pwm_duty_cycles->num_duty_pcts << 1);
			led->pwm_pause_lo = on_ms %
				(led->pwm_duty_cycles->num_duty_pcts << 1);
		} else {
			led->pwm_duty_cycles->duty_ms = on_ms /
				(led->pwm_duty_cycles->num_duty_pcts);
			led->pwm_pause_lo = on_ms %
				(led->pwm_duty_cycles->num_duty_pcts);
		}
		led->pwm_pause_hi = total_ms - on_ms;
		dev_dbg(led->cdev.dev, "duty_ms %d, pause_hi %d, pause_lo %d, total_ms %d, on_ms %d\n",
				led->pwm_duty_cycles->duty_ms, led->pwm_pause_hi, led->pwm_pause_lo,
				total_ms, on_ms);
	}

	pwm_max = pm8xxx_adjust_brightness(&led->cdev, led->cdev.brightness);
	start_idx = led->pwm_duty_cycles->start_idx;
#endif

#ifdef LGE_PWM_LED
	idx_len0 = led->pwm_duty_cycles->num_duty_pcts0;
	idx_len1 = led->pwm_duty_cycles->num_duty_pcts1;
	pcts0 = led->pwm_duty_cycles->duty_pcts0;
	pcts1 = led->pwm_duty_cycles->duty_pcts1;
#else
	idx_len = led->pwm_duty_cycles->num_duty_pcts;
	pcts = led->pwm_duty_cycles->duty_pcts;
#endif


#ifdef LGE_PWM_LED
	if (led->blink) {
		mid0 = (idx_len0 - 1) >> 1;
		for (i = 0; i <= mid0; i++) {
			temp = ((pwm_max * i) << 1) / mid0 + 1;
			pcts0[i] = temp >> 1;
			pcts0[idx_len0 - 1 - i] = temp >> 1;
		}

		mid1 = (idx_len1 - 1) >> 1;
		for (i = 0; i <= mid1; i++) {
			temp = ((pwm_max * i) << 1) / mid1 + 1;
			pcts1[i] = temp >> 1;
			pcts1[idx_len1 - 1 - i] = temp >> 1;
		}
	} else {
		for (i = 0; i < idx_len0; i++) {
			pcts0[i] = pwm_max;
		}
		for (i = 0; i < idx_len1; i++) {
			pcts1[i] = pwm_max;
		}
	}
#else
	if (led->blink) {
		mid = (idx_len - 1) >> 1;
		for (i = 0; i <= mid; i++) {
			temp = ((pwm_max * i) << 1) / mid + 1;
			pcts[i] = temp >> 1;
			pcts[idx_len - 1 - i] = temp >> 1;
		}
	} else {
		for (i = 0; i < idx_len; i++) {
			pcts[i] = pwm_max;
		}
	}
#endif

#ifdef LGE_PWM_LED
	if (idx_len0 >= PM_PWM_LUT_SIZE && start_idx) {
		pr_err("Wrong LUT size or index\n");
		return -EINVAL;
	}
	if ((start_idx + idx_len0) > PM_PWM_LUT_SIZE) {
		pr_err("Exceed LUT limit\n");
		return -EINVAL;
	}
	if (idx_len1 >= PM_PWM_LUT_SIZE && start_idx) {
		pr_err("Wrong LUT size or index\n");
		return -EINVAL;
	}
	if ((start_idx + idx_len1) > PM_PWM_LUT_SIZE) {
		pr_err("Exceed LUT limit\n");
		return -EINVAL;
	}

	rc = pm8xxx_pwm_lut_config(led->pwm_dev, led->pwm_period_us,
			led->pwm_duty_cycles->duty_pcts0,
			led->pwm_duty_cycles->duty_ms0,
			start_idx, idx_len0, led->pwm_pause_lo, led->pwm_pause_hi,
			PM8XXX_LED_PWM_FLAGS);
#else
	if (idx_len >= PM_PWM_LUT_SIZE && start_idx) {
		pr_err("Wrong LUT size or index\n");
		return -EINVAL;
	}
	if ((start_idx + idx_len) > PM_PWM_LUT_SIZE) {
		pr_err("Exceed LUT limit\n");
		return -EINVAL;
	}

	rc = pm8xxx_pwm_lut_config(led->pwm_dev, led->pwm_period_us,
			led->pwm_duty_cycles->duty_pcts,
			led->pwm_duty_cycles->duty_ms,
			start_idx, idx_len, led->pwm_pause_lo, led->pwm_pause_hi,
			PM8XXX_LED_PWM_FLAGS);
#endif

	return rc;
}

static int pm8xxx_led_pwm_work(struct pm8xxx_led_data *led)
{
	int duty_us;
	int rc = 0;
	int level = 0;

	level = pm8xxx_adjust_brightness(&led->cdev, led->cdev.brightness);

	if (led->pwm_duty_cycles == NULL) {
		duty_us = (led->pwm_period_us * level) / LED_FULL;
		rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
		if (led->cdev.brightness) {
			led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.brightness);
			rc = pwm_enable(led->pwm_dev);
		} else {
			pwm_disable(led->pwm_dev);
			led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1,
				led->cdev.brightness);
		}
	} else {
		if (level) {
			pm8xxx_led_pwm_pattern_update(led);
			led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, level);
		}

		rc = pm8xxx_pwm_lut_enable(led->pwm_dev, level);
		if (!level)
			led_rgb_write(led, SSBI_REG_ADDR_RGB_CNTL1, level);
	}

	return rc;
}

static void __pm8xxx_led_work(struct pm8xxx_led_data *led,
					enum led_brightness value)
{
	int rc;
	int level = 0;

	mutex_lock(&led->lock);

	level = pm8xxx_adjust_brightness(&led->cdev, value);

	switch (led->id) {
	case PM8XXX_ID_LED_KB_LIGHT:
		led_kp_set(led, level);
		break;
	case PM8XXX_ID_LED_0:
	case PM8XXX_ID_LED_1:
	case PM8XXX_ID_LED_2:
		led_lc_set(led, level);
		break;
	case PM8XXX_ID_FLASH_LED_0:
	case PM8XXX_ID_FLASH_LED_1:
		led_flash_set(led, level);
		break;
	case PM8XXX_ID_WLED:
		rc = led_wled_set(led, level);
		if (rc < 0)
			pr_err("wled brightness set failed %d\n", rc);
		break;
	case PM8XXX_ID_RGB_LED_RED:
	case PM8XXX_ID_RGB_LED_GREEN:
	case PM8XXX_ID_RGB_LED_BLUE:
		led_rgb_set(led, level);
		break;
	default:
		dev_err(led->cdev.dev, "unknown led id %d", led->id);
		break;
	}

	mutex_unlock(&led->lock);
}

static void pm8xxx_led_work(struct work_struct *work)
{
	int rc;

	struct pm8xxx_led_data *led = container_of(work,
					 struct pm8xxx_led_data, work);

	dev_dbg(led->cdev.dev, "led %s set %d (%s mode)\n",
			led->cdev.name, led->cdev.brightness,
			(led->pwm_dev ? "pwm" : "manual"));

	if (led->pwm_dev == NULL) {
		__pm8xxx_led_work(led, led->cdev.brightness);
	} else {
		rc = pm8xxx_led_pwm_work(led);
		if (rc)
			pr_err("could not configure PWM mode for LED:%d\n",
								led->id);
	}
}

static void pm8xxx_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	struct	pm8xxx_led_data *led;

#ifdef LGE_PWM_LED
	int idx_len0;
	int idx_len1;
#endif

	led = container_of(led_cdev, struct pm8xxx_led_data, cdev);

	Ledlog("LedLog > %s : %d %d\n",__func__, led->id, value);
		
	#ifdef LGE_PWM_LED
	if(led->id == PM8XXX_ID_LED_2 || led->id == PM8XXX_ID_LED_0)
	{
		idx_len0 = led->pwm_duty_cycles->num_duty_pcts0;
		idx_len1 = led->pwm_duty_cycles->num_duty_pcts1;

		if(value == 0xFE) //notification
		{
			pm8xxx_pwm_lut_config(led->pwm_dev, led->pwm_period_us,
				led->pwm_duty_cycles->duty_pcts1,
				led->pwm_duty_cycles->duty_ms1,
				0, idx_len1, led->pwm_pause_lo, led->pwm_pause_hi,
				PM8XXX_LED_PWM_FLAGS);
			value = led->cdev.max_brightness;
		}
		else //charging
		{
			pm8xxx_pwm_lut_config(led->pwm_dev, led->pwm_period_us,
				led->pwm_duty_cycles->duty_pcts0,
				led->pwm_duty_cycles->duty_ms0,
				0, idx_len0, led->pwm_pause_lo, led->pwm_pause_hi,
				PM8XXX_LED_PWM_FLAGS);
		}
	}
	#endif

	if (value < LED_OFF || value > led->cdev.max_brightness) {
		dev_err(led->cdev.dev, "Invalid brightness value exceeds");
		return;
	}

	if (!led->lock_update) {
		schedule_work(&led->work);
	} else {
		dev_dbg(led->cdev.dev, "set %d pending\n",
				value);
	}
}

static int pm8xxx_set_led_mode_and_adjust_brightness(struct pm8xxx_led_data *led,
		enum pm8xxx_led_modes led_mode, int max_current)
{
	switch (led->id) {
	case PM8XXX_ID_LED_0:
	case PM8XXX_ID_LED_1:
	case PM8XXX_ID_LED_2:
		led->adjust_brightness = max_current /
			PM8XXX_ID_LED_CURRENT_FACTOR;
		if (led->adjust_brightness > MAX_LC_LED_BRIGHTNESS)
			led->adjust_brightness = MAX_LC_LED_BRIGHTNESS;
		led->reg = led_mode;
		break;
	case PM8XXX_ID_LED_KB_LIGHT:
	case PM8XXX_ID_FLASH_LED_0:
	case PM8XXX_ID_FLASH_LED_1:
		led->adjust_brightness = max_current /
						PM8XXX_ID_FLASH_CURRENT_FACTOR;
		if (led->adjust_brightness > MAX_FLASH_BRIGHTNESS)
			led->adjust_brightness = MAX_FLASH_BRIGHTNESS;

		switch (led_mode) {
		case PM8XXX_LED_MODE_PWM1:
		case PM8XXX_LED_MODE_PWM2:
		case PM8XXX_LED_MODE_PWM3:
			led->reg = PM8XXX_FLASH_MODE_PWM;
			break;
		case PM8XXX_LED_MODE_DTEST1:
			led->reg = PM8XXX_FLASH_MODE_DBUS1;
			break;
		case PM8XXX_LED_MODE_DTEST2:
			led->reg = PM8XXX_FLASH_MODE_DBUS2;
			break;
		default:
			led->reg = PM8XXX_LED_MODE_MANUAL;
			break;
		}
		break;
	case PM8XXX_ID_WLED:
		led->adjust_brightness = WLED_MAX_LEVEL;
		break;
	case PM8XXX_ID_RGB_LED_RED:
	case PM8XXX_ID_RGB_LED_GREEN:
	case PM8XXX_ID_RGB_LED_BLUE:
		break;
	default:
		dev_err(led->cdev.dev, "LED Id is invalid");
		return -EINVAL;
	}

	return 0;
}

static enum led_brightness pm8xxx_led_get(struct led_classdev *led_cdev)
{
	struct pm8xxx_led_data *led;

	led = container_of(led_cdev, struct pm8xxx_led_data, cdev);

	return led->cdev.brightness;
}

static int __devinit init_wled(struct pm8xxx_led_data *led)
{
	int rc, i;
	u8 val, num_wled_strings;

	num_wled_strings = led->wled_cfg->num_strings;

	/* program over voltage protection threshold */
	if (led->wled_cfg->ovp_val > WLED_OVP_37V) {
		dev_err(led->dev->parent, "Invalid ovp value");
		return -EINVAL;
	}

	rc = pm8xxx_readb(led->dev->parent, WLED_OVP_CFG_REG, &val);
	if (rc) {
		dev_err(led->dev->parent, "can't read wled ovp config"
			" register rc=%d\n", rc);
		return rc;
	}

	val = (val & ~WLED_OVP_VAL_MASK) |
		(led->wled_cfg->ovp_val << WLED_OVP_VAL_BIT_SHFT);

	rc = pm8xxx_writeb(led->dev->parent, WLED_OVP_CFG_REG, val);
	if (rc) {
		dev_err(led->dev->parent, "can't write wled ovp config"
			" register rc=%d\n", rc);
		return rc;
	}

	/* program current boost limit and output feedback*/
	if (led->wled_cfg->boost_curr_lim > WLED_CURR_LIMIT_1680mA) {
		dev_err(led->dev->parent, "Invalid boost current limit");
		return -EINVAL;
	}

	rc = pm8xxx_readb(led->dev->parent, WLED_BOOST_CFG_REG, &val);
	if (rc) {
		dev_err(led->dev->parent, "can't read wled boost config"
			" register rc=%d\n", rc);
		return rc;
	}

	val = (val & ~WLED_BOOST_LIMIT_MASK) |
		(led->wled_cfg->boost_curr_lim << WLED_BOOST_LIMIT_BIT_SHFT);

	val = (val & ~WLED_OP_FDBCK_MASK) |
		(led->wled_cfg->op_fdbck << WLED_OP_FDBCK_BIT_SHFT);

	rc = pm8xxx_writeb(led->dev->parent, WLED_BOOST_CFG_REG, val);
	if (rc) {
		dev_err(led->dev->parent, "can't write wled boost config"
			" register rc=%d\n", rc);
		return rc;
	}

	/* program high pole capacitance */
	if (led->wled_cfg->cp_select > WLED_CP_SELECT_MAX) {
		dev_err(led->dev->parent, "Invalid pole capacitance");
		return -EINVAL;
	}

	rc = pm8xxx_readb(led->dev->parent, WLED_HIGH_POLE_CAP_REG, &val);
	if (rc) {
		dev_err(led->dev->parent, "can't read wled high pole"
			" capacitance register rc=%d\n", rc);
		return rc;
	}

	val = (val & ~WLED_CP_SELECT_MASK) | led->wled_cfg->cp_select;

	rc = pm8xxx_writeb(led->dev->parent, WLED_HIGH_POLE_CAP_REG, val);
	if (rc) {
		dev_err(led->dev->parent, "can't write wled high pole"
			" capacitance register rc=%d\n", rc);
		return rc;
	}

	/* program activation delay and maximum current */
	for (i = 0; i < num_wled_strings; i++) {
		rc = pm8xxx_readb(led->dev->parent,
				WLED_MAX_CURR_CFG_REG(i + 2), &val);
		if (rc) {
			dev_err(led->dev->parent, "can't read wled max current"
				" config register rc=%d\n", rc);
			return rc;
		}

		if ((led->wled_cfg->ctrl_delay_us % WLED_CTL_DLY_STEP) ||
			(led->wled_cfg->ctrl_delay_us > WLED_CTL_DLY_MAX)) {
			dev_err(led->dev->parent, "Invalid control delay\n");
			return rc;
		}

		val = val / WLED_CTL_DLY_STEP;
		val = (val & ~WLED_CTL_DLY_MASK) |
			(led->wled_cfg->ctrl_delay_us << WLED_CTL_DLY_BIT_SHFT);

		if ((led->max_current > WLED_MAX_CURR)) {
			dev_err(led->dev->parent, "Invalid max current\n");
			return -EINVAL;
		}

		val = (val & ~WLED_MAX_CURR_MASK) | led->max_current;

		rc = pm8xxx_writeb(led->dev->parent,
				WLED_MAX_CURR_CFG_REG(i + 2), val);
		if (rc) {
			dev_err(led->dev->parent, "can't write wled max current"
				" config register rc=%d\n", rc);
			return rc;
		}
	}

	/* program digital module generator, cs out and enable the module */
	rc = pm8xxx_readb(led->dev->parent, WLED_MOD_CTRL_REG, &val);
	if (rc) {
		dev_err(led->dev->parent, "can't read wled module ctrl"
			" register rc=%d\n", rc);
		return rc;
	}

	if (led->wled_cfg->dig_mod_gen_en)
		val |= WLED_DIG_MOD_GEN_MASK;

	if (led->wled_cfg->cs_out_en)
		val |= WLED_CS_OUT_MASK;

	val |= WLED_EN_MASK;

	rc = pm8xxx_writeb(led->dev->parent, WLED_MOD_CTRL_REG, val);
	if (rc) {
		dev_err(led->dev->parent, "can't write wled module ctrl"
			" register rc=%d\n", rc);
		return rc;
	}
	led->wled_mod_ctrl_val = val;

	/* dump wled registers */
	wled_dump_regs(led);

	return 0;
}

static int __devinit get_init_value(struct pm8xxx_led_data *led, u8 *val)
{
	int rc, offset;
	u16 addr;

	switch (led->id) {
	case PM8XXX_ID_LED_KB_LIGHT:
		addr = SSBI_REG_ADDR_DRV_KEYPAD;
		break;
	case PM8XXX_ID_LED_0:
	case PM8XXX_ID_LED_1:
	case PM8XXX_ID_LED_2:
		offset = PM8XXX_LED_OFFSET(led->id);
		addr = SSBI_REG_ADDR_LED_CTRL(offset);
		break;
	case PM8XXX_ID_FLASH_LED_0:
		addr = SSBI_REG_ADDR_FLASH_DRV0;
		break;
	case PM8XXX_ID_FLASH_LED_1:
		addr = SSBI_REG_ADDR_FLASH_DRV1;
		break;
	case PM8XXX_ID_WLED:
		rc = init_wled(led);
		if (rc)
			dev_err(led->cdev.dev, "can't initialize wled rc=%d\n",
								rc);
		return rc;
	case PM8XXX_ID_RGB_LED_RED:
	case PM8XXX_ID_RGB_LED_GREEN:
	case PM8XXX_ID_RGB_LED_BLUE:
		addr = SSBI_REG_ADDR_RGB_CNTL1;
		break;
	default:
		dev_err(led->cdev.dev, "unknown led id %d", led->id);
		return -EINVAL;
	}

	rc = pm8xxx_readb(led->dev->parent, addr, val);
	if (rc)
		dev_err(led->cdev.dev, "can't get led(%d) level rc=%d\n",
							led->id, rc);

	return rc;
}

static int pm8xxx_led_pwm_configure(struct pm8xxx_led_data *led)
{
	int duty_us, rc;

	led->pwm_dev = pwm_request(led->pwm_channel,
					led->cdev.name);

	if (IS_ERR_OR_NULL(led->pwm_dev)) {
		pr_err("could not acquire PWM Channel %d, "
			"error %ld\n", led->pwm_channel,
			PTR_ERR(led->pwm_dev));
		led->pwm_dev = NULL;
		return -ENODEV;
	}

	if (led->pwm_duty_cycles != NULL) {
		rc = pm8xxx_led_pwm_pattern_update(led);
	} else {
		duty_us = led->pwm_period_us;
		rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
	}

	return rc;
}

static ssize_t pm8xxx_led_lock_update_show(struct device *dev,
		                struct device_attribute *attr, char *buf)
{
	const struct pm8xxx_led_platform_data *pdata = dev->platform_data;
	struct pm8xxx_led_data *leds =  (struct pm8xxx_led_data *)dev_get_drvdata(dev);
	int i, n = 0;

	for (i = 0; i < pdata->num_configs; i++)
	{
		n += sprintf(&buf[n], "%s is %s\n",
					leds[i].cdev.name,
					(leds[i].lock_update ? "non-updatable" : "updatable"));
	}

	return n;
}

static ssize_t pm8xxx_led_lock_update_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t size)
{
	struct pm8xxx_led_platform_data *pdata = dev->platform_data;
	struct pm8xxx_led_data *leds =  (struct pm8xxx_led_data *)dev_get_drvdata(dev);
	ssize_t rc = -EINVAL;
	char *after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;
	int i;

	if (isspace(*after))
		count++;

	if (count == size) {
		rc = count;
		for (i = 0; i < pdata->num_configs; i++)
		{
			leds[i].lock_update = state;
			if (!state) {
				dev_info(dev, "resume %s set %d\n",
						leds[i].cdev.name, leds[i].cdev.brightness);
				schedule_work(&leds[i].work);
			}
		}
	}
	return rc;
}

static DEVICE_ATTR(lock, 0644, pm8xxx_led_lock_update_show, pm8xxx_led_lock_update_store);

static ssize_t pm8xxx_led_grppwm_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	const struct pm8xxx_led_platform_data *pdata = dev->platform_data;
	struct pm8xxx_led_data *leds =  (struct pm8xxx_led_data *)dev_get_drvdata(dev);
	int i,  n = 0;

	for (i = 0; i < pdata->num_configs; i++)
	{
		n += sprintf(&buf[n], "%s period_us is %d\n", leds[i].cdev.name, leds[i].pwm_grppwm);
	}
	return n;
}

static ssize_t pm8xxx_led_grppwm_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	const struct pm8xxx_led_platform_data *pdata = dev->platform_data;
	struct pm8xxx_led_data *leds =  (struct pm8xxx_led_data *)dev_get_drvdata(dev);
	ssize_t rc = -EINVAL;
	char *after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;
	int i;

	if (isspace(*after))
		count++;

	if (count == size) {
		rc = count;
		if (state < 0)
			state = 0;
		if (state > PM8XXX_LED_PWM_GRPPWM_MAX)
			state = PM8XXX_LED_PWM_GRPPWM_MAX;

		for (i = 0; i < pdata->num_configs; i++)
		{
			leds[i].pwm_grppwm = state;
			dev_dbg(leds[i].cdev.dev, "set grppwm %lu\n", state);
		}
	}
	return rc;
}

static DEVICE_ATTR(grppwm, 0644, pm8xxx_led_grppwm_show, pm8xxx_led_grppwm_store);

static ssize_t pm8xxx_led_grpfreq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	const struct pm8xxx_led_platform_data *pdata = dev->platform_data;
	struct pm8xxx_led_data *leds =  (struct pm8xxx_led_data *)dev_get_drvdata(dev);
	int i,  n = 0;

	for (i = 0; i < pdata->num_configs; i++)
	{
		n += sprintf(&buf[n], "%s freq %d\n", leds[i].cdev.name, leds[i].pwm_grpfreq);
	}
	return n;
}

static ssize_t pm8xxx_led_grpfreq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	const struct pm8xxx_led_platform_data *pdata = dev->platform_data;
	struct pm8xxx_led_data *leds =  (struct pm8xxx_led_data *)dev_get_drvdata(dev);
	ssize_t rc = -EINVAL;
	char *after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;
	int i;

	if (isspace(*after))
		count++;

	if (count == size) {
		rc = count;

		if(state < 0)
			state = 0;
		if(state > PM8XXX_LED_PWM_GRPFREQ_MAX)
			state = PM8XXX_LED_PWM_GRPFREQ_MAX;

		for (i = 0; i < pdata->num_configs; i++)
		{
			leds[i].pwm_grpfreq = state;
			dev_dbg(leds[i].cdev.dev, "set grpfreq %lu\n", state);
		}
	}
	return rc;
}

static DEVICE_ATTR(grpfreq, 0644, pm8xxx_led_grpfreq_show, pm8xxx_led_grpfreq_store);

static ssize_t pm8xxx_led_blink_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	const struct pm8xxx_led_platform_data *pdata = dev->platform_data;
	struct pm8xxx_led_data *leds =  (struct pm8xxx_led_data *)dev_get_drvdata(dev);
	int i, blink = 0;

	for (i = 0; i < pdata->num_configs; i++)
	{
		if (leds[i].blink)
			blink |= 1 << i;
	}
	return sprintf(buf, "%d", blink);
}

static ssize_t pm8xxx_led_blink_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	const struct pm8xxx_led_platform_data *pdata = dev->platform_data;
	struct pm8xxx_led_data *leds =  (struct pm8xxx_led_data *)dev_get_drvdata(dev);
	ssize_t rc = -EINVAL;
	char *after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;
	int i;

	if (isspace(*after))
		count++;

	if (count == size) {
		rc = count;

		for (i = 0; i < pdata->num_configs; i++)
		{
			if (leds[i].blink != state) {
				leds[i].blink = state;
				if(!leds[i].lock_update)
					rc = pm8xxx_led_pwm_pattern_update(&leds[i]);
			}
		}
	}
	return rc;
}

static DEVICE_ATTR(blink, 0644, pm8xxx_led_blink_show, pm8xxx_led_blink_store);


static int __devinit pm8xxx_led_probe(struct platform_device *pdev)
{
	const struct pm8xxx_led_platform_data *pdata = pdev->dev.platform_data;
	const struct led_platform_data *pcore_data;
	struct led_info *curr_led;
	struct pm8xxx_led_data *led, *led_dat;
	struct pm8xxx_led_config *led_cfg;
	enum pm8xxx_version version;
	bool found = false;
	int rc, i, j;

	if (pdata == NULL) {
		dev_err(&pdev->dev, "platform data not supplied\n");
		return -EINVAL;
	}

	pcore_data = pdata->led_core;

	if (pcore_data->num_leds != pdata->num_configs) {
		dev_err(&pdev->dev, "#no. of led configs and #no. of led"
				"entries are not equal\n");
		return -EINVAL;
	}

	led = kcalloc(pcore_data->num_leds, sizeof(*led), GFP_KERNEL);
	if (led == NULL) {
		dev_err(&pdev->dev, "failed to alloc memory\n");
		return -ENOMEM;
	}

	for (i = 0; i < pcore_data->num_leds; i++) {
		curr_led	= &pcore_data->leds[i];
		led_dat		= &led[i];
		led_cfg		= &pdata->configs[i];

		led_dat->id     = led_cfg->id;
		led_dat->pwm_channel = led_cfg->pwm_channel;
		led_dat->pwm_period_us = led_cfg->pwm_period_us;
		led_dat->pwm_duty_cycles = led_cfg->pwm_duty_cycles;
		led_dat->wled_cfg = led_cfg->wled_cfg;
		led_dat->max_current = led_cfg->max_current;
		led_dat->lock_update = 0;
		led_dat->blink = 0;

		if (!((led_dat->id >= PM8XXX_ID_LED_KB_LIGHT) &&
				(led_dat->id < PM8XXX_ID_MAX))) {
			dev_err(&pdev->dev, "invalid LED ID(%d) specified\n",
				led_dat->id);
			rc = -EINVAL;
			goto fail_id_check;

		}

		found = false;
		version = pm8xxx_get_version(pdev->dev.parent);
		for (j = 0; j < ARRAY_SIZE(led_map); j++) {
			if (version == led_map[j].version
			&& (led_map[j].supported & (1 << led_dat->id))) {
				found = true;
				break;
			}
		}

		if (!found) {
			dev_err(&pdev->dev, "invalid LED ID(%d) specified\n",
				led_dat->id);
			rc = -EINVAL;
			goto fail_id_check;
		}

		led_dat->cdev.name		= curr_led->name;
		led_dat->cdev.default_trigger   = curr_led->default_trigger;
		led_dat->cdev.brightness_set    = pm8xxx_led_set;
		led_dat->cdev.brightness_get    = pm8xxx_led_get;
		led_dat->cdev.max_brightness    = LED_FULL;
		led_dat->cdev.brightness	= LED_OFF;
		led_dat->cdev.flags		= curr_led->flags;
		led_dat->dev			= &pdev->dev;

		rc =  get_init_value(led_dat, &led_dat->reg);
		if (rc < 0)
			goto fail_id_check;

		rc = pm8xxx_set_led_mode_and_adjust_brightness(led_dat,
					led_cfg->mode, led_cfg->max_current);
		if (rc < 0)
			goto fail_id_check;

		mutex_init(&led_dat->lock);
		INIT_WORK(&led_dat->work, pm8xxx_led_work);

		rc = led_classdev_register(&pdev->dev, &led_dat->cdev);
		if (rc) {
			dev_err(&pdev->dev, "unable to register led %d,rc=%d\n",
						 led_dat->id, rc);
			goto fail_id_check;
		}

		/* configure default state */
		if (led_cfg->default_state)
			led_dat->cdev.brightness = led_dat->cdev.max_brightness;
		else
			led_dat->cdev.brightness = LED_OFF;

		if (led_cfg->mode != PM8XXX_LED_MODE_MANUAL) {
			if (led_dat->id == PM8XXX_ID_RGB_LED_RED ||
				led_dat->id == PM8XXX_ID_RGB_LED_GREEN ||
				led_dat->id == PM8XXX_ID_RGB_LED_BLUE)
				__pm8xxx_led_work(led_dat, 0);
			else
				__pm8xxx_led_work(led_dat,
					led_dat->cdev.max_brightness);

			if (led_dat->pwm_channel != -1) {
				if (led_cfg->pwm_adjust_brightness) {
					led_dat->adjust_brightness = led_cfg->pwm_adjust_brightness;
				} else {
					led_dat->adjust_brightness = 100;
				}

				rc = pm8xxx_led_pwm_configure(led_dat);
				if (rc) {
					dev_err(&pdev->dev, "failed to "
					"configure LED, error: %d\n", rc);
					goto fail_id_check;
				}
				schedule_work(&led->work);
			}
		} else {
			__pm8xxx_led_work(led_dat, led_dat->cdev.brightness);
		}
	}

	led->use_pwm = pdata->use_pwm;

	platform_set_drvdata(pdev, led);

	rc = device_create_file(&pdev->dev, &dev_attr_lock);
	if (rc) {
		device_remove_file(&pdev->dev, &dev_attr_lock);
		dev_err(&pdev->dev, "failed device_create_file(lock)\n");
	}

	if (led->use_pwm) {
		rc = device_create_file(&pdev->dev, &dev_attr_blink);
		if (rc) {
			device_remove_file(&pdev->dev, &dev_attr_blink);
			dev_err(&pdev->dev, "failed device_create_file(blink)\n");
		}

		rc = device_create_file(&pdev->dev, &dev_attr_grppwm);
		if (rc) {
			device_remove_file(&pdev->dev, &dev_attr_grppwm);
			dev_err(&pdev->dev, "failed device_create_fiaild(grppwm)\n");
		}

		rc = device_create_file(&pdev->dev, &dev_attr_grpfreq);
		if (rc) {
			device_remove_file(&pdev->dev, &dev_attr_grpfreq);
			dev_err(&pdev->dev, "failed device_create_file(grpfreq)\n");
		}
	}

	return 0;

fail_id_check:
	if (i > 0) {
		for (i = i - 1; i >= 0; i--) {
			mutex_destroy(&led[i].lock);
			led_classdev_unregister(&led[i].cdev);
			if (led[i].pwm_dev != NULL)
				pwm_free(led[i].pwm_dev);
		}
	}
	kfree(led);
	return rc;
}

static int __devexit pm8xxx_led_remove(struct platform_device *pdev)
{
	int i;
	const struct led_platform_data *pdata =
				pdev->dev.platform_data;
	struct pm8xxx_led_data *led = platform_get_drvdata(pdev);

	for (i = 0; i < pdata->num_leds; i++) {
		cancel_work_sync(&led[i].work);
		mutex_destroy(&led[i].lock);
		led_classdev_unregister(&led[i].cdev);
		if (led[i].pwm_dev != NULL) {
			pwm_free(led[i].pwm_dev);
		}
	}

	device_remove_file(&pdev->dev, &dev_attr_lock);

	if (led->use_pwm) {
		device_remove_file(&pdev->dev, &dev_attr_blink);
		device_remove_file(&pdev->dev, &dev_attr_grppwm);
		device_remove_file(&pdev->dev, &dev_attr_grpfreq);
	}

	kfree(led);

	return 0;
}

static struct platform_driver pm8xxx_led_driver = {
	.probe		= pm8xxx_led_probe,
	.remove		= __devexit_p(pm8xxx_led_remove),
	.driver		= {
		.name	= PM8XXX_LEDS_DEV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init pm8xxx_led_init(void)
{
	return platform_driver_register(&pm8xxx_led_driver);
}
subsys_initcall(pm8xxx_led_init);

static void __exit pm8xxx_led_exit(void)
{
	platform_driver_unregister(&pm8xxx_led_driver);
}
module_exit(pm8xxx_led_exit);

MODULE_DESCRIPTION("PM8XXX LEDs driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:pm8xxx-led");
