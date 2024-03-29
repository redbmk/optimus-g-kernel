/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/debugfs.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/mfd/pmic8058.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <mach/mdm2.h>
#include <mach/restart.h>
#include <mach/subsystem_notif.h>
#include <mach/subsystem_restart.h>
#include <linux/msm_charm.h>
#include "msm_watchdog.h"
#include "devices.h"
#include "clock.h"
#include "mdm_private.h"

#ifdef CONFIG_LGE_PM_LOW_BATT_CHG
#include <mach/board_lge.h>
#endif

#define MDM_PBLRDY_CNT		20

static int mdm_debug_mask;
static int power_on_count;
static int hsic_peripheral_status;
static DEFINE_MUTEX(hsic_status_lock);


static void mdm_peripheral_connect(struct mdm_modem_drv *mdm_drv)
{
	if (!mdm_drv->pdata->peripheral_platform_device)
		return;

	mutex_lock(&hsic_status_lock);
	if (hsic_peripheral_status)
		goto out;
	platform_device_add(mdm_drv->pdata->peripheral_platform_device);
	hsic_peripheral_status = 1;
out:
	mutex_unlock(&hsic_status_lock);
}

static void mdm_peripheral_disconnect(struct mdm_modem_drv *mdm_drv)
{
	if (!mdm_drv->pdata->peripheral_platform_device)
		return;

	mutex_lock(&hsic_status_lock);
	if (!hsic_peripheral_status)
		goto out;
	platform_device_del(mdm_drv->pdata->peripheral_platform_device);
	hsic_peripheral_status = 0;
out:
	mutex_unlock(&hsic_status_lock);
}

/* This function can be called from atomic context. */
static void mdm_toggle_soft_reset(struct mdm_modem_drv *mdm_drv)
{
	int soft_reset_direction_assert = 0,
	    soft_reset_direction_de_assert = 1;

	if (mdm_drv->pdata->soft_reset_inverted) {
		soft_reset_direction_assert = 1;
		soft_reset_direction_de_assert = 0;
	}
	gpio_direction_output(mdm_drv->ap2mdm_soft_reset_gpio,
			soft_reset_direction_assert);
	/* Use mdelay because this function can be called from atomic
	 * context.
	 */
	mdelay(10);
	gpio_direction_output(mdm_drv->ap2mdm_soft_reset_gpio,
			soft_reset_direction_de_assert);
}

/* This function can be called from atomic context. */
static void mdm_atomic_soft_reset(struct mdm_modem_drv *mdm_drv)
{
	mdm_toggle_soft_reset(mdm_drv);
}

static void mdm_power_down_common(struct mdm_modem_drv *mdm_drv)
{
	int i;
	int soft_reset_direction =
		mdm_drv->pdata->soft_reset_inverted ? 1 : 0;

	mdm_peripheral_disconnect(mdm_drv);

	/* Wait for the modem to complete its power down actions. */
	for (i = 20; i > 0; i--) {
		if (gpio_get_value(mdm_drv->mdm2ap_status_gpio) == 0) {
			if (mdm_debug_mask & MDM_DEBUG_MASK_SHDN_LOG)
				pr_info("%s: mdm2ap_status went low, i = %d\n",
					__func__, i);
			break;
		}
		msleep(100);
	}

#ifdef CONFIG_LGE_PM_LOW_BATT_CHG
	/* temp fix : mdm power off failure when chargerlogo exit. force down mdm ps-hold */
	if( i == 0 || lge_get_charger_logo_state() )
#else
	if (i == 0)
#endif
	{
		pr_err("%s: MDM2AP_STATUS never went low. Doing a hard reset\n",
			   __func__);
		gpio_direction_output(mdm_drv->ap2mdm_soft_reset_gpio,
					soft_reset_direction);
		/*
		* Currently, there is a debounce timer on the charm PMIC. It is
		* necessary to hold the PMIC RESET low for ~3.5 seconds
		* for the reset to fully take place. Sleep here to ensure the
		* reset has occured before the function exits.
		*/
		msleep(4000);
	}
}

static void mdm_do_first_power_on(struct mdm_modem_drv *mdm_drv)
{
	int i;
	int pblrdy;
	if (power_on_count != 1) {
		pr_err("%s: Calling fn when power_on_count != 1\n",
			   __func__);
		return;
	}

	pr_err("%s: Powering on modem for the first time\n", __func__);
	mdm_peripheral_disconnect(mdm_drv);

	/* If this is the first power-up after a panic, the modem may still
	 * be in a power-on state, in which case we need to toggle the gpio
	 * instead of just de-asserting it. No harm done if the modem was
	 * powered down.
	 */
	mdm_toggle_soft_reset(mdm_drv);
	/* If the device has a kpd pwr gpio then toggle it. */
	if (GPIO_IS_VALID(mdm_drv->ap2mdm_kpdpwr_n_gpio)) {
		/* Pull AP2MDM_KPDPWR gpio high and wait for PS_HOLD to settle,
		 * then	pull it back low.
		 */
		pr_debug("%s: Pulling AP2MDM_KPDPWR gpio high\n", __func__);
		gpio_direction_output(mdm_drv->ap2mdm_kpdpwr_n_gpio, 1);
		msleep(1000);
		gpio_direction_output(mdm_drv->ap2mdm_kpdpwr_n_gpio, 0);
	}

	if (!GPIO_IS_VALID(mdm_drv->mdm2ap_pblrdy))
		goto start_mdm_peripheral;

	for (i = 0; i  < MDM_PBLRDY_CNT; i++) {
		pblrdy = gpio_get_value(mdm_drv->mdm2ap_pblrdy);
		if (pblrdy)
			break;
		usleep_range(5000, 5000);
	}
	pr_debug("%s: i:%d\n", __func__, i);

start_mdm_peripheral:
	mdm_peripheral_connect(mdm_drv);
	msleep(200);
    pr_info("%s--\n", __func__);
}

static void mdm_do_soft_power_on(struct mdm_modem_drv *mdm_drv)
{
	int i;
	int pblrdy;

	pr_err("%s: soft resetting mdm modem\n", __func__);
	mdm_peripheral_disconnect(mdm_drv);
	mdm_toggle_soft_reset(mdm_drv);

	if (!GPIO_IS_VALID(mdm_drv->mdm2ap_pblrdy))
		goto start_mdm_peripheral;

	for (i = 0; i  < MDM_PBLRDY_CNT; i++) {
		pblrdy = gpio_get_value(mdm_drv->mdm2ap_pblrdy);
		if (pblrdy)
			break;
		usleep_range(5000, 5000);
	}

	pr_debug("%s: i:%d\n", __func__, i);

start_mdm_peripheral:
	mdm_peripheral_connect(mdm_drv);
	msleep(200);
}

static void mdm_power_on_common(struct mdm_modem_drv *mdm_drv)
{
	power_on_count++;

	/* this gpio will be used to indicate apq readiness,
	 * de-assert it now so that it can be asserted later.
	 * May not be used.
	 */
	if (GPIO_IS_VALID(mdm_drv->ap2mdm_wakeup_gpio))
		gpio_direction_output(mdm_drv->ap2mdm_wakeup_gpio, 0);

	/*
	 * If we did an "early power on" then ignore the very next
	 * power-on request because it would the be first request from
	 * user space but we're already powered on. Ignore it.
	 */
	if (mdm_drv->pdata->early_power_on &&
			(power_on_count == 2))
		return;

	if (power_on_count == 1)
		mdm_do_first_power_on(mdm_drv);
	else
		mdm_do_soft_power_on(mdm_drv);
}

static void debug_state_changed(int value)
{
	mdm_debug_mask = value;
}

static void mdm_status_changed(struct mdm_modem_drv *mdm_drv, int value)
{
	pr_debug("%s: value:%d\n", __func__, value);

	if (value) {
		mdm_peripheral_disconnect(mdm_drv);
		mdm_peripheral_connect(mdm_drv);
		if (GPIO_IS_VALID(mdm_drv->ap2mdm_wakeup_gpio))
			gpio_direction_output(mdm_drv->ap2mdm_wakeup_gpio, 1);
	}
}

static void mdm_image_upgrade(struct mdm_modem_drv *mdm_drv, int type)
{
	switch (type) {
	case APQ_CONTROLLED_UPGRADE:
		pr_debug("%s APQ controlled modem image upgrade\n", __func__);
		mdm_drv->mdm_ready = 0;
		mdm_toggle_soft_reset(mdm_drv);
		break;
	case MDM_CONTROLLED_UPGRADE:
		pr_debug("%s MDM controlled modem image upgrade\n", __func__);
		mdm_drv->mdm_ready = 0;
		/*
		 * If we have no image currently present on the modem, then we
		 * would be in PBL, in which case the status gpio would not go
		 * high.
		 */
		mdm_drv->disable_status_check = 1;
		if (GPIO_IS_VALID(mdm_drv->usb_switch_gpio)) {
			pr_info("%s Switching usb control to MDM\n", __func__);
			gpio_direction_output(mdm_drv->usb_switch_gpio, 1);
		} else
			pr_err("%s usb switch gpio unavailable\n", __func__);
		break;
	default:
		pr_err("%s invalid upgrade type\n", __func__);
	}
}
static struct mdm_ops mdm_cb = {
	.power_on_mdm_cb = mdm_power_on_common,
	.reset_mdm_cb = mdm_power_on_common,
	.atomic_reset_mdm_cb = mdm_atomic_soft_reset,
	.power_down_mdm_cb = mdm_power_down_common,
	.debug_state_changed_cb = debug_state_changed,
	.status_cb = mdm_status_changed,
	.image_upgrade_cb = mdm_image_upgrade,
};

static int __init mdm_modem_probe(struct platform_device *pdev)
{
	return mdm_common_create(pdev, &mdm_cb);
}

static int __devexit mdm_modem_remove(struct platform_device *pdev)
{
	return mdm_common_modem_remove(pdev);
}

static void mdm_modem_shutdown(struct platform_device *pdev)
{
	mdm_common_modem_shutdown(pdev);
}

static struct platform_driver mdm_modem_driver = {
	.remove         = mdm_modem_remove,
	.shutdown	= mdm_modem_shutdown,
	.driver         = {
		.name = "mdm2_modem",
		.owner = THIS_MODULE
	},
};

static int __init mdm_modem_init(void)
{
	return platform_driver_probe(&mdm_modem_driver, mdm_modem_probe);
}

static void __exit mdm_modem_exit(void)
{
	platform_driver_unregister(&mdm_modem_driver);
}

module_init(mdm_modem_init);
module_exit(mdm_modem_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("mdm modem driver");
MODULE_VERSION("2.0");
MODULE_ALIAS("mdm_modem");
