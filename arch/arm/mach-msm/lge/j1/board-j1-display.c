/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/bootmem.h>
#include <linux/ion.h>
#include <asm/mach-types.h>
#include <mach/msm_memtypes.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/ion.h>
#include <mach/msm_bus_board.h>
#include <mach/socinfo.h>

#include "devices.h"
#include "board-j1.h"

#include "../../../../drivers/video/msm/msm_fb.h"
#include "../../../../drivers/video/msm/msm_fb_def.h"
#include "../../../../drivers/video/msm/mipi_dsi.h"

#include <mach/board_lge.h>

//20120413 LGE_UPDATE_S hojin.ryu@lge.com 
#include <linux/i2c.h>
#include <linux/kernel.h>
//20120413 LGE_UPDATE_E hojin.ryu@lge.com 

//#define CONFIG_LGE_BACKLIGHT_CABC
// DSDR_START
#ifndef LGE_DSDR_SUPPORT
#define LGE_DSDR_SUPPORT
#endif
// DSDR_END

#ifdef CONFIG_LCD_KCAL
struct kcal_data kcal_value;
#endif

#ifdef CONFIG_UPDATE_LCDC_LUT
extern unsigned int lcd_color_preset_lut[];
int update_preset_lcdc_lut(void)
{
	struct fb_cmap cmap;
	int ret = 0;

	cmap.start = 0;
	cmap.len = 256;
	cmap.transp = NULL;

#ifdef CONFIG_LCD_KCAL
	cmap.red = (uint16_t *)&(kcal_value.red);
	cmap.green = (uint16_t *)&(kcal_value.green);
	cmap.blue = (uint16_t *)&(kcal_value.blue);
#else
	cmap.red = NULL;
	cmap.green = NULL;
	cmap.blue = NULL;
#endif

	ret = mdp_preset_lut_update_lcdc(&cmap, lcd_color_preset_lut);
	if (ret)
		pr_err("%s: failed to set lut! %d\n", __func__, ret);

	return ret;
}
#endif

#ifdef CONFIG_FB_MSM_TRIPLE_BUFFER
/* prim = 1366 x 768 x 3(bpp) x 3(pages) */
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WXGA_PT)
#define MSM_FB_PRIM_BUF_SIZE roundup(768 * 1280 * 4 * 3, 0x10000)
#elif defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_INVERSE_PT)
#define MSM_FB_PRIM_BUF_SIZE roundup(1088 *1920 * 4 * 3, 0x10000)
#elif defined(CONFIG_FB_MSM_MIPI_HITACHI_VIDEO_HD_PT)
#define MSM_FB_PRIM_BUF_SIZE roundup(736 * 1280 * 4 * 3, 0x10000)
#else
#define MSM_FB_PRIM_BUF_SIZE roundup(1920 * 1088 * 4 * 3, 0x10000)
#endif
#else
/* prim = 1366 x 768 x 3(bpp) x 2(pages) */
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WXGA_PT)
#define MSM_FB_PRIM_BUF_SIZE roundup(768 * 1280 * 4 * 2, 0x10000)
#elif defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_INVERSE_PT)
#define MSM_FB_PRIM_BUF_SIZE roundup(1088 *1920 * 4 * 2, 0x10000)
#elif definded(CONFIG_FB_MSM_MIPI_HITACHI_VIDEO_HD_PT)
#define MSM_FB_PRIM_BUF_SIZE roundup(736 * 1280 * 4 * 2, 0x10000)
#else
#define MSM_FB_PRIM_BUF_SIZE roundup(1920 * 1088 * 4 * 2, 0x10000)
#endif
#endif /*CONFIG_FB_MSM_TRIPLE_BUFFER */

#ifdef LGE_DSDR_SUPPORT
/* LGE_CHANGE
 * [DSDR]
 * 2012-04-12, sebastian.song@lge.com
 */
#define MSM_FB_EXT_BUF_SIZE \
        (roundup((1920 * 1088 * 4), 4096) * 3) /* 4 bpp x 3 page */
#else  /* LGE_DSDR_SUPPORT */
#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
#define MSM_FB_EXT_BUF_SIZE \
		(roundup((1920 * 1088 * 2), 4096) * 1) /* 2 bpp x 1 page */
#elif defined(CONFIG_FB_MSM_TVOUT)
#define MSM_FB_EXT_BUF_SIZE \
		(roundup((720 * 576 * 2), 4096) * 2) /* 2 bpp x 2 pages */
#else
#define MSM_FB_EXT_BUF_SIZE	0
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL */
#endif //LGE_DSDR_SUPPORT

#ifdef CONFIG_FB_MSM_WRITEBACK_MSM_PANEL
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_INVERSE_PT)
#define MSM_FB_WFD_BUF_SIZE \
		(roundup((1920 * 1088 * 2), 4096) * 3) /* 2 bpp x 3 page */
#else		
#define MSM_FB_WFD_BUF_SIZE \
		(roundup((1280 * 736 * 2), 4096) * 3) /* 2 bpp x 3 page */
#endif
#else
#define MSM_FB_WFD_BUF_SIZE     0
#endif

#define MSM_FB_SIZE \
	roundup(MSM_FB_PRIM_BUF_SIZE + \
		MSM_FB_EXT_BUF_SIZE + MSM_FB_WFD_BUF_SIZE, 4096)

#ifdef CONFIG_FB_MSM_OVERLAY0_WRITEBACK
	#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WXGA_PT)
	#define MSM_FB_OVERLAY0_WRITEBACK_SIZE roundup((768 * 1280 * 3 * 2), 4096)
	#elif defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_INVERSE_PT)
	#define MSM_FB_OVERLAY0_WRITEBACK_SIZE roundup((1088 * 1920 * 3 * 2), 4096)
	#elif defined(CONFIG_FB_MSM_MIPI_HITACHI_VIDEO_HD_PT)
	#define MSM_FB_OVERLAY0_WRITEBACK_SIZE roundup((736 * 1280 * 3 * 2), 4096)
	#else
	#define MSM_FB_OVERLAY0_WRITEBACK_SIZE (0)
	#endif
#else
#define MSM_FB_OVERLAY0_WRITEBACK_SIZE (0)
#endif  /* CONFIG_FB_MSM_OVERLAY0_WRITEBACK */

#ifdef CONFIG_FB_MSM_OVERLAY1_WRITEBACK
#define MSM_FB_OVERLAY1_WRITEBACK_SIZE roundup((1920 * 1088 * 3 * 2), 4096)
#else
#define MSM_FB_OVERLAY1_WRITEBACK_SIZE (0)
#endif  /* CONFIG_FB_MSM_OVERLAY1_WRITEBACK */

#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WXGA_PT)
#define LGIT_IEF
#endif
//LGE_UPDATE_S  hojin.ryu@lge.com 20120629 IEF Switch define for camera preview
#define LGIT_IEF_SWITCH
//LGE_UPDATE_S  hojin.ryu@lge.com 20120629
static struct resource msm_fb_resources[] = {
	{
		.flags = IORESOURCE_DMA,
	}
};

#define MIPI_VIDEO_TOSHIBA_WSVGA_PANEL_NAME "mipi_video_toshiba_wsvga"
#define MIPI_VIDEO_CHIMEI_WXGA_PANEL_NAME "mipi_video_chimei_wxga"
#define HDMI_PANEL_NAME "hdmi_msm"
#define TVOUT_PANEL_NAME "tvout_msm"

static int msm_fb_detect_panel(const char *name)
{
	return 0;
}

static struct msm_fb_platform_data msm_fb_pdata = {
	.detect_client = msm_fb_detect_panel,
};

static struct platform_device msm_fb_device = {
	.name              = "msm_fb",
	.id                = 0,
	.num_resources     = ARRAY_SIZE(msm_fb_resources),
	.resource          = msm_fb_resources,
	.dev.platform_data = &msm_fb_pdata,
};

void __init apq8064_allocate_fb_region(void)
{
	void *addr;
	unsigned long size;

	size = MSM_FB_SIZE;
	addr = alloc_bootmem_align(size, 0x1000);
	msm_fb_resources[0].start = __pa(addr);
	msm_fb_resources[0].end = msm_fb_resources[0].start + size - 1;
	pr_info("allocating %lu bytes at %p (%lx physical) for fb\n",
			size, addr, __pa(addr));
}

#define MDP_VSYNC_GPIO 0

#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_INVERSE_PT)
static struct msm_bus_vectors mdp_1080p_vectors[] = {
	/* 1080p and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 2000000000,
		.ib = 2000000000,
	},
};

static struct msm_bus_paths mdp_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(mdp_1080p_vectors),
		mdp_1080p_vectors,
	},
	{
		ARRAY_SIZE(mdp_1080p_vectors),
		mdp_1080p_vectors,
	},
	{
		ARRAY_SIZE(mdp_1080p_vectors),
		mdp_1080p_vectors,
	},
	{
		ARRAY_SIZE(mdp_1080p_vectors),
		mdp_1080p_vectors,
	},
	{
		ARRAY_SIZE(mdp_1080p_vectors),
		mdp_1080p_vectors,
	},
	{
		ARRAY_SIZE(mdp_1080p_vectors),
		mdp_1080p_vectors,
	},
};
static struct msm_bus_scale_pdata mdp_bus_scale_pdata = {
	mdp_bus_scale_usecases,
	ARRAY_SIZE(mdp_bus_scale_usecases),
	.name = "mdp",
};

#define MDP_MAX_CLK_RATE 266667000

#else	

static struct msm_bus_vectors mdp_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors mdp_ui_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 216000000 * 2,
	    .ib = 270000000 * 2,
	},
};

static struct msm_bus_vectors mdp_vga_vectors[] = {
	/* VGA and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 216000000 * 2,
		.ib = 270000000 * 2,
	},
};

static struct msm_bus_vectors mdp_720p_vectors[] = {
	/* 720p and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 230400000 * 2,
		.ib = 288000000 * 2,
	},
};

static struct msm_bus_vectors mdp_1080p_vectors[] = {
	/* 1080p and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 334080000 * 2,
		.ib = 417600000 * 2,
	},
};

static struct msm_bus_paths mdp_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(mdp_init_vectors),
		mdp_init_vectors,
	},
	{
		ARRAY_SIZE(mdp_ui_vectors),
		mdp_ui_vectors,
	},
	{
		ARRAY_SIZE(mdp_ui_vectors),
		mdp_ui_vectors,
	},
	{
		ARRAY_SIZE(mdp_vga_vectors),
		mdp_vga_vectors,
	},
	{
		ARRAY_SIZE(mdp_720p_vectors),
		mdp_720p_vectors,
	},
	{
		ARRAY_SIZE(mdp_1080p_vectors),
		mdp_1080p_vectors,
	},
};

static struct msm_bus_scale_pdata mdp_bus_scale_pdata = {
	mdp_bus_scale_usecases,
	ARRAY_SIZE(mdp_bus_scale_usecases),
	.name = "mdp",
};

#define MDP_MAX_CLK_RATE 200000000

#endif

static struct msm_panel_common_pdata mdp_pdata = {
	.gpio = MDP_VSYNC_GPIO,
        .mdp_max_clk = MDP_MAX_CLK_RATE,
	.mdp_bus_scale_table = &mdp_bus_scale_pdata,
	.mdp_rev = MDP_REV_44,
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	.mem_hid = BIT(ION_CP_MM_HEAP_ID),
#else
	.mem_hid = MEMTYPE_EBI1,
#endif
	.mdp_iommu_split_domain = 1,
/* LGE_UPDATE_S 2012-05-10 jungbeom.shim@lge.com  for early backlight on for APQ8064 */
	.cont_splash_enabled = 0x01,
/* LGE_UPDATE_E 2012-05-10 jungbeom.shim@lge.com  for early backlight on for APQ8064  */
};

void __init apq8064_mdp_writeback(struct memtype_reserve* reserve_table)
{
	mdp_pdata.ov0_wb_size = MSM_FB_OVERLAY0_WRITEBACK_SIZE;
	mdp_pdata.ov1_wb_size = MSM_FB_OVERLAY1_WRITEBACK_SIZE;
#if defined(CONFIG_ANDROID_PMEM) && !defined(CONFIG_MSM_MULTIMEDIA_USE_ION)
	reserve_table[mdp_pdata.mem_hid].size +=
		mdp_pdata.ov0_wb_size;
	reserve_table[mdp_pdata.mem_hid].size +=
		mdp_pdata.ov1_wb_size;
#endif
}

#ifdef CONFIG_LCD_KCAL
int kcal_set_values(int kcal_r, int kcal_g, int kcal_b)
{
	kcal_value.red = kcal_r;
	kcal_value.green = kcal_g;
	kcal_value.blue = kcal_b;
	return 0;
}

static int kcal_get_values(int *kcal_r, int *kcal_g, int *kcal_b)
{
	*kcal_r = kcal_value.red;
	*kcal_g = kcal_value.green;
	*kcal_b = kcal_value.blue;
	return 0;
}

static int kcal_refresh_values(void)
{
	return update_preset_lcdc_lut();
}

static struct kcal_platform_data kcal_pdata = {
	.set_values = kcal_set_values,
	.get_values = kcal_get_values,
	.refresh_display = kcal_refresh_values
};

static struct platform_device kcal_platrom_device = {
	.name   = "kcal_ctrl",
	.dev = {
		.platform_data = &kcal_pdata,
	}
};
#endif

static struct resource hdmi_msm_resources[] = {
	{
		.name  = "hdmi_msm_qfprom_addr",
		.start = 0x00700000,
		.end   = 0x007060FF,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = "hdmi_msm_hdmi_addr",
		.start = 0x04A00000,
		.end   = 0x04A00FFF,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = "hdmi_msm_irq",
		.start = HDMI_IRQ,
		.end   = HDMI_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static int hdmi_enable_5v(int on);
static int hdmi_core_power(int on, int show);
static int hdmi_cec_power(int on);
static int hdmi_gpio_config(int on);
static int hdmi_panel_power(int on);

static struct msm_hdmi_platform_data hdmi_msm_data = {
	.irq = HDMI_IRQ,
	.enable_5v = hdmi_enable_5v,
	.core_power = hdmi_core_power,
	.cec_power = hdmi_cec_power,
	.panel_power = hdmi_panel_power,
	.gpio_config = hdmi_gpio_config,
};

static struct platform_device hdmi_msm_device = {
	.name = "hdmi_msm",
	.id = 0,
	.num_resources = ARRAY_SIZE(hdmi_msm_resources),
	.resource = hdmi_msm_resources,
	.dev.platform_data = &hdmi_msm_data,
};

static char wfd_check_mdp_iommu_split_domain(void)
{
	return mdp_pdata.mdp_iommu_split_domain;
}

#ifdef CONFIG_FB_MSM_WRITEBACK_MSM_PANEL
static struct msm_wfd_platform_data wfd_pdata = {
	.wfd_check_mdp_iommu_split = wfd_check_mdp_iommu_split_domain,
};

static struct platform_device wfd_panel_device = {
	.name = "wfd_panel",
	.id = 0,
	.dev.platform_data = NULL,
};

static struct platform_device wfd_device = {
	.name          = "msm_wfd",
	.id            = -1,
	.dev.platform_data = &wfd_pdata,
};
#endif

/* HDMI related GPIOs */
#define HDMI_CEC_VAR_GPIO	69
#define HDMI_DDC_CLK_GPIO	70
#define HDMI_DDC_DATA_GPIO	71
#define HDMI_HPD_GPIO		72

#define DSV_LOAD_EN 86
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_INVERSE_PT)
#define DSV_ONBST	57 // GPIO(APQ) - DSV_EN
#endif

static bool dsi_power_on;
static int mipi_dsi_panel_power(int on)
{
	static struct regulator *reg_l8, *reg_l2, *reg_lvs6;
/* LGE_UPDATE_S 2012-05-18 hj.eum@lge.com  for ev.B of japaness vendor(temporary) */
#if defined(CONFIG_FB_MSM_MIPI_HITACHI_VIDEO_HD_PT)
	static int gpio20;	// LCD RST GPIO for rev.B
#endif
/* LGE_UPDATE_E 2012-05-18 hj.eum@lge.com  for ev.B of japaness vendor(temporary) */
	static int gpio42;
	int rc;

/* LGE_UPDATE_S 2012-05-18 hj.eum@lge.com  for ev.B of japaness vendor(temporary) */
#if defined(CONFIG_FB_MSM_MIPI_HITACHI_VIDEO_HD_PT)
	// LCD RST GPIO for rev.B
	struct pm_gpio gpio20_param = {
			.direction = PM_GPIO_DIR_OUT,
			.output_buffer = PM_GPIO_OUT_BUF_CMOS,
			.output_value = 0,
			.pull = PM_GPIO_PULL_NO,
			.vin_sel = 2,
			.out_strength = PM_GPIO_STRENGTH_HIGH,
			.function = PM_GPIO_FUNC_PAIRED,
			.inv_int_pol = 0,
			.disable_pin = 0,
		};
#endif
/* LGE_UPDATE_E 2012-05-18 hj.eum@lge.com  for ev.B of japaness vendor(temporary) */
	struct pm_gpio gpio42_param = {
			.direction = PM_GPIO_DIR_OUT,
			.output_buffer = PM_GPIO_OUT_BUF_CMOS,
			.output_value = 0,
			.pull = PM_GPIO_PULL_NO,
			.vin_sel = 2,
			.out_strength = PM_GPIO_STRENGTH_HIGH,
			.function = PM_GPIO_FUNC_PAIRED,
			.inv_int_pol = 0,
			.disable_pin = 0,
		};
	printk(KERN_INFO"%s: mipi lcd function started status = %d \n", __func__, on);

	pr_debug("%s: state : %d\n", __func__, on);

	if (!dsi_power_on) {

/* LGE_UPDATE_S 2012-05-18 hj.eum@lge.com  for ev.B of japaness vendor(temporary) */
#if defined(CONFIG_FB_MSM_MIPI_HITACHI_VIDEO_HD_PT)
		// LCD RST GPIO for rev.B
		if (lge_get_board_revno() == HW_REV_B)
		{
			gpio20 = PM8921_GPIO_PM_TO_SYS(20);

			rc = gpio_request(gpio20, "disp_rst_n");
			if (rc) {
				pr_err("request gpio 20 failed, rc=%d\n", rc);
				return -ENODEV;
			}
		}
		else
		{
			gpio42 = PM8921_GPIO_PM_TO_SYS(42);

			rc = gpio_request(gpio42, "disp_rst_n");
			if (rc) {
				pr_err("request gpio 42 failed, rc=%d\n", rc);
				return -ENODEV;
			}
		}
#else
		rc = gpio_request(DSV_LOAD_EN,"DSV_LOAD_EN");  //GPIO_86
		if (rc)
		{
			printk(KERN_INFO "%s: DSV_LAOD_EN gpio_86 Request Fail \n", __func__);
		}
		else
		{
			rc = gpio_direction_output(DSV_LOAD_EN, 1);	 // OUTPUT
			if (rc) {
				pr_err("request gpio 86 failed, rc=%d\n", rc);
				return -ENODEV;
			}
		}
		gpio42 = PM8921_GPIO_PM_TO_SYS(42);

		rc = gpio_request(gpio42, "disp_rst_n");
		if (rc) {
			pr_err("request gpio 42 failed, rc=%d\n", rc);
			return -ENODEV;
		}
#endif
/* LGE_UPDATE_E 2012-05-18 hj.eum@lge.com  for ev.B of japaness vendor(temporary) */
		
		reg_l8 = regulator_get(&msm_mipi_dsi1_device.dev, "dsi_vci");
		if (IS_ERR(reg_l8)) {
			pr_err("could not get 8921_l8, rc = %ld\n",
				PTR_ERR(reg_l8));
			return -ENODEV;
		}
	
		reg_lvs6 = regulator_get(&msm_mipi_dsi1_device.dev, "dsi_iovcc");
		if (IS_ERR(reg_lvs6)) {
			pr_err("could not get 8921_lvs6, rc = %ld\n",
				 PTR_ERR(reg_lvs6));
		      return -ENODEV;
		   }

		reg_l2 = regulator_get(&msm_mipi_dsi1_device.dev, "dsi_vdda");
		if (IS_ERR(reg_l2)) {
			pr_err("could not get 8921_l2, rc = %ld\n",
				PTR_ERR(reg_l2));
			return -ENODEV;
		}

		rc = regulator_set_voltage(reg_l8, 3000000, 3000000);
		if (rc) {
			pr_err("set_voltage l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		
		rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
		if (rc) {
			pr_err("set_voltage l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}

		dsi_power_on = true;
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_INVERSE_PT)
		mipi_dsi_panel_power(0);
		mdelay(50);
#endif
	}
	if (on) {

#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WXGA_PT)
		gpio_set_value_cansleep(DSV_LOAD_EN, 1);
		mdelay(1);
#endif
		rc = regulator_set_optimum_mode(reg_l8, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}
      
		rc = regulator_set_optimum_mode(reg_l2, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
		
#if defined(CONFIG_FB_MSM_MIPI_HITACHI_VIDEO_HD_PT)
		rc = regulator_enable(reg_lvs6); // IOVCC
		if (rc) {
			pr_err("enable lvs6 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		udelay(100);

		rc = regulator_enable(reg_l8);	// dsi_vci
			if (rc) {
				pr_err("enable l8 failed, rc=%d\n", rc);
				return -ENODEV;
			}

		udelay(100);

		rc = regulator_enable(reg_l2);	// DSI
		if (rc) {
			pr_err("enable l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}
#elif defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WXGA_PT)
		rc = regulator_enable(reg_l8);  // dsi_vci
		if (rc) {
			pr_err("enable l8 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		
		udelay(100);

		rc = regulator_enable(reg_lvs6); // IOVCC
		if (rc) {
			pr_err("enable lvs6 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		udelay(100);
#elif defined (CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_INVERSE_PT)

		udelay(100);

		rc = regulator_enable(reg_lvs6); // IOVCC
		if (rc) {
			pr_err("enable lvs6 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		mdelay(2);
		
		rc = gpio_request(DSV_ONBST,"DSV_ONBST_en");
		
		if (rc) {
			printk(KERN_INFO "%s: DSV_ONBST Request Fail \n", __func__);
		} else {
		rc = gpio_direction_output(DSV_ONBST, 1);   // OUTPUT
		if (rc) {
			printk(KERN_INFO "%s: DSV_ONBST Direction Set Fail \n", __func__);
		}
		else {
			gpio_set_value(DSV_ONBST, 1);
		}
		gpio_free(DSV_ONBST);
		
		mdelay(2);
		}
			

#endif
		
		rc = regulator_enable(reg_l2);  // DSI
		if (rc) {
			pr_err("enable l2 failed, rc=%d\n", rc);
			return -ENODEV;
		}

		printk(KERN_INFO " %s : reset start.", __func__);
/* LGE_UPDATE_S 2012-05-18 hj.eum@lge.com  for ev.B of japaness vendor(temporary) */
#if defined(CONFIG_FB_MSM_MIPI_HITACHI_VIDEO_HD_PT)
		// LCD RESET HIGH for rev.B
		if (lge_get_board_revno() == HW_REV_B)
		{
			mdelay(2);
			gpio20_param.output_value = 1;
			rc = pm8xxx_gpio_config(gpio20,&gpio20_param);	
			if (rc) {
				pr_err("gpio_config 20 failed (3), rc=%d\n", rc);
				return -EINVAL;
			}
		}
		else
		{
			mdelay(2);
			gpio42_param.output_value = 1;
			rc = pm8xxx_gpio_config(gpio42,&gpio42_param);	
			if (rc) {
				pr_err("gpio_config 42 failed (3), rc=%d\n", rc);
				return -EINVAL;
			}
			mdelay(11);
		}
#else
		// LCD RESET HIGH
		mdelay(2);
		gpio42_param.output_value = 1;
		rc = pm8xxx_gpio_config(gpio42,&gpio42_param);	
				if (rc) {
			pr_err("gpio_config 42 failed (3), rc=%d\n", rc);
			return -EINVAL;
		}
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_INVERSE_PT)
		mdelay(60);
#else
		mdelay(5);
#endif
#endif
/* LGE_UPDATE_E 2012-05-18 hj.eum@lge.com  for ev.B of japaness vendor(temporary) */

	} else {
/* LGE_UPDATE_S 2012-05-18 hj.eum@lge.com  for ev.B of japaness vendor(temporary) */
#if defined(CONFIG_FB_MSM_MIPI_HITACHI_VIDEO_HD_PT)
		// LCD RESET LOW for rev.B
		if (lge_get_board_revno() == HW_REV_B)
		{
			gpio20_param.output_value = 0;
			rc = pm8xxx_gpio_config(gpio20,&gpio20_param);
			if (rc) {
				pr_err("gpio_config 20 failed, rc=%d\n", rc);
				return -ENODEV;
			}
		}
		else
		{
			// DGMS : MC-C05717-2
			// LCD RESET LOW
			gpio42_param.output_value = 0;
			rc = pm8xxx_gpio_config(gpio42,&gpio42_param);
			if (rc) {
				pr_err("gpio_config 42 failed, rc=%d\n", rc);
				return -ENODEV;
			}
			udelay(100);
		}
#else
		// DGMS : MC-C05717-2
		// LCD RESET LOW
		gpio42_param.output_value = 0;
		rc = pm8xxx_gpio_config(gpio42,&gpio42_param);
		if (rc) {
			pr_err("gpio_config 42 failed, rc=%d\n", rc);
			return -ENODEV;
		}
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_INVERSE_PT)
		mdelay(2);
#else
		udelay(100);
#endif
#endif
/* LGE_UPDATE_E 2012-05-18 hj.eum@lge.com  for ev.B of japaness vendor(temporary) */
#if defined(CONFIG_FB_MSM_MIPI_HITACHI_VIDEO_HD_PT)
		rc = regulator_disable(reg_l8);	//VCI
		if (rc) {
			pr_err("disable reg_l8  failed, rc=%d\n", rc);
			return -ENODEV;
		}

		rc = regulator_disable(reg_lvs6);	// IOVCC
		if (rc) {
			pr_err("disable lvs6 failed, rc=%d\n", rc);
			return -ENODEV;
		}
#elif defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WXGA_PT)
		rc = regulator_disable(reg_lvs6);	// IOVCC
		if (rc) {
			pr_err("disable lvs6 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		udelay(100);
			
		rc = regulator_disable(reg_l8);	//VCI
		if (rc) {
			pr_err("disable reg_l8  failed, rc=%d\n", rc);
			return -ENODEV;
		}
		udelay(100);
#elif defined (CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_INVERSE_PT)
		rc = gpio_request(DSV_ONBST,"DSV_ONBST_en");

		if (rc) {
			printk(KERN_INFO "%s: DSV_ONBST Request Fail \n", __func__);
		} 
		else
		{
				
			rc = gpio_direction_output(DSV_ONBST, 1);   // OUTPUT
			if (rc) {
				printk(KERN_INFO "%s: DSV_ONBST Direction Set Fail \n", __func__);
			}
			else {
				gpio_set_value(DSV_ONBST, 0);  
			}
			
			gpio_free(DSV_ONBST); 
		}

		mdelay(2);
		
		rc = regulator_disable(reg_lvs6);	// IOVCC
		if (rc) {
			pr_err("disable lvs6 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		mdelay(2);
			

#endif
		rc = regulator_disable(reg_l2);	//DSI
		if (rc) {
			pr_err("disable reg_l2  failed, rc=%d\n", rc);
			return -ENODEV;
		}

#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_INVERSE_PT)
/* GK/GV not use l8 */
#else
		rc = regulator_set_optimum_mode(reg_l8, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l8 failed, rc=%d\n", rc);
			return -EINVAL;
		}
#endif

		rc = regulator_set_optimum_mode(reg_l2, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			return -EINVAL;
		}
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WXGA_PT)
			mdelay(1);
			gpio_set_value_cansleep(DSV_LOAD_EN, 0);
#endif
	}
	
	return 0;
}

/* LGE_UPDATE_S 2012-05-10 jungbeom.shim@lge.com  for early backlight on for APQ8064 */
static char mipi_dsi_splash_is_enabled(void)
{
	return mdp_pdata.cont_splash_enabled;
}
/* LGE_UPDATE_E 2012-05-10 jungbeom.shim@lge.com  for early backlight on for APQ8064 */
static struct mipi_dsi_platform_data mipi_dsi_pdata = {
	.dsi_power_save = mipi_dsi_panel_power,
/* LGE_UPDATE_S 2012-05-10 jungbeom.shim@lge.com  for early backlight on for APQ8064 */
	.splash_is_enabled = mipi_dsi_splash_is_enabled,
/* LGE_UPDATE_E 2012-05-10 jungbeom.shim@lge.com  for early backlight on for APQ8064 */
};

static struct msm_bus_vectors dtv_bus_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors dtv_bus_def_vectors[] = {
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_INVERSE_PT)
		.ab = 2000000000,
		.ib = 2000000000,
#else	
		.ab = 566092800 * 2,
		.ib = 707616000 * 2,
#endif
	},
};

static struct msm_bus_paths dtv_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(dtv_bus_init_vectors),
		dtv_bus_init_vectors,
	},
	{
		ARRAY_SIZE(dtv_bus_def_vectors),
		dtv_bus_def_vectors,
	},
};
static struct msm_bus_scale_pdata dtv_bus_scale_pdata = {
	dtv_bus_scale_usecases,
	ARRAY_SIZE(dtv_bus_scale_usecases),
	.name = "dtv",
};

static struct lcdc_platform_data dtv_pdata = {
	.bus_scale_table = &dtv_bus_scale_pdata,
	.lcdc_power_save = hdmi_panel_power,
};

static int hdmi_panel_power(int on)
{
	int rc;

	pr_debug("%s: HDMI Core: %s\n", __func__, (on ? "ON" : "OFF"));
	rc = hdmi_core_power(on, 1);
	if (rc)
		rc = hdmi_cec_power(on);

	pr_debug("%s: HDMI Core: %s Success\n", __func__, (on ? "ON" : "OFF"));
	return rc;
}

static int hdmi_enable_5v(int on)
{
	return 0;
}

static int hdmi_core_power(int on, int show)
{
	static struct regulator *reg_8921_lvs7;
	static int prev_on;
	int rc;

	if (on == prev_on)
		return 0;

	if (!reg_8921_lvs7) {
		reg_8921_lvs7 = regulator_get(&hdmi_msm_device.dev,
					      "hdmi_vdda");
		if (IS_ERR(reg_8921_lvs7)) {
			pr_err("could not get reg_8921_lvs7, rc = %ld\n",
				PTR_ERR(reg_8921_lvs7));
			reg_8921_lvs7 = NULL;
			return -ENODEV;
		}
	}

	if (on) {

		rc = regulator_enable(reg_8921_lvs7);
		if (rc) {
			pr_err("'%s' regulator enable failed, rc=%d\n",
				"hdmi_vdda", rc);
			goto error1;
		}
		
		pr_debug("%s(on): success\n", __func__);
	} else {
		rc = regulator_disable(reg_8921_lvs7);
		if (rc) {
			pr_err("disable reg_8921_l23 failed, rc=%d\n", rc);
			return -ENODEV;
		}
		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;

	return 0;

error1:
	return rc;
}

static int hdmi_gpio_config(int on)
{
	int rc = 0;
	static int prev_on;
//	int pmic_gpio14 = PM8921_GPIO_PM_TO_SYS(14);

	if (on == prev_on)
		return 0;

	if (on) {
		rc = gpio_request(HDMI_DDC_CLK_GPIO, "HDMI_DDC_CLK");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_DDC_CLK", HDMI_DDC_CLK_GPIO, rc);
			goto error1;
		}
		rc = gpio_request(HDMI_DDC_DATA_GPIO, "HDMI_DDC_DATA");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_DDC_DATA", HDMI_DDC_DATA_GPIO, rc);
			goto error2;
		}
		rc = gpio_request(HDMI_HPD_GPIO, "HDMI_HPD");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_HPD", HDMI_HPD_GPIO, rc);
			goto error3;
		}
		pr_debug("%s(on): success\n", __func__);

	} else {
		gpio_free(HDMI_DDC_CLK_GPIO);
		gpio_free(HDMI_DDC_DATA_GPIO);
		gpio_free(HDMI_HPD_GPIO);

		pr_debug("%s(off): success\n", __func__);
	}

	prev_on = on;

	return 0;

error3:
	gpio_free(HDMI_DDC_DATA_GPIO);
error2:
	gpio_free(HDMI_DDC_CLK_GPIO);
error1:
	return rc;	
}

static int hdmi_cec_power(int on)
{
	return 0;
}

#if defined (CONFIG_LGE_BACKLIGHT_LM3530)
extern void lm3530_lcd_backlight_set_level( int level);
#elif defined (CONFIG_LGE_BACKLIGHT_LM3533)
extern void lm3533_lcd_backlight_set_level( int level);
#endif

#if defined(CONFIG_FB_MSM_MIPI_HITACHI_VIDEO_HD_PT)
static int mipi_hitachi_backlight_level(int level, int max, int min)
{ 
	lm3533_lcd_backlight_set_level(level);
	return 0;
}

/* HITACHI 4.67" HD panel */
static char set_address_mode[2] = {0x36, 0x00};
static char set_pixel_format[2] = {0x3A, 0x70};

static char exit_sleep[2] = {0x11, 0x00};
static char display_on[2] = {0x29, 0x00};
static char enter_sleep[2] = {0x10, 0x00};
static char display_off[2] = {0x28, 0x00};

static char macp_off[2] = {0xB0, 0x04};
static char macp_on[2] = {0xB0, 0x03};

#if defined(CONFIG_LGE_BACKLIGHT_CABC)
#define CABC_POWERON_OFFSET 4 /* offset from lcd display on cmds */

#define CABC_OFF 0
#define CABC_ON 1

#define CABC_10 1
#define CABC_20 2
#define CABC_30 3
#define CABC_40 4
#define CABC_50 5

#define CABC_DEFUALT CABC_10

#if defined (CONFIG_LGE_BACKLIGHT_CABC_DEBUG)
static int hitachi_cabc_index = CABC_DEFUALT;
#endif

static char backlight_ctrl1[2][6] = {

	/* off */
	{
		0xB8, 0x00, 0x00, 0x30,
		0x18, 0x18
	},
	/* on */
	{
		0xB8, 0x01, 0x00, 0x30,
		0x18, 0x18
	},
};

static char backlight_ctrl2[6][8] = {
	/* off */
	{
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00
	},
	/* 10% */
	{
		0xB9, 0x18, 0x00, 0x18,
		0x18, 0x9F, 0x1F, 0x0F
	},

	/* 20% */
	{
		0xB9, 0x18, 0x00, 0x18,
		0x18, 0x9F, 0x1F, 0x0F
	},

	/* 30% */
	{
		0xB9, 0x18, 0x00, 0x18,
		0x18, 0x9F, 0x1F, 0x0F
	},

	/* 40% */
	{
		0xB9, 0x18, 0x00, 0x18,
		0x18, 0x9F, 0x1F, 0x0F
	},
	/* 50% */
	{
		0xB9, 0x18, 0x00, 0x18,
		0x18, 0x9F, 0x1F, 0x0F
	}
};

static char backlight_ctrl3[6][25] = {
	/* off */
	{
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00
	},
	/* 10% */
	{
		0xBA, 0x00, 0x00, 0x0C,
		0x12, 0x6C, 0x11, 0x6C,
		0x12, 0x0C, 0x12, 0x00,
		0xDA, 0x6D, 0x03, 0xFF,
		0xFF, 0x10, 0x67, 0xA3,
		0xDB, 0xFB, 0xFF, 0x9F,
		0x00
	},
	/* 20% */
	{
		0xBA, 0x00, 0x00, 0x0C,
		0x0B, 0x6C, 0x0B, 0xAC,
		0x0B, 0x0C, 0x0B, 0x00,
		0xDA, 0x6D, 0x03, 0xFF,
		0xFF, 0x10, 0xB3, 0xC9,
		0xDC, 0xEE, 0xFF, 0x9F,
		0x00
	},
	/* 30% */
	{
		0xBA, 0x00, 0x00, 0x0C,
		0x0D, 0x6C, 0x0D, 0xAC,
		0x0D, 0x0C, 0x0D, 0x00,
		0xDA, 0x6D, 0x03, 0xFF,
		0xFF, 0x10, 0x8C, 0xAA,
		0xC7, 0xE3, 0xFF, 0x9F,
		0x00
	},
	/* 40% */
	{
		0xBA, 0x00, 0x00, 0x0C,
		0x13, 0xAC, 0x13, 0x6C,
		0x13, 0x0C, 0x13, 0x00,
		0xDA, 0x6D, 0x03, 0xFF,
		0xFF, 0x10, 0x67, 0x89,
		0xAF, 0xD6, 0xFF, 0x9F,
		0x00
	},
	/* 50% */
	{
		0xBA, 0x00, 0x00, 0x0C,
		0x14, 0xAC, 0x14, 0x6C,
		0x14, 0x0C, 0x14, 0x00,
		0xDA, 0x6D, 0x03, 0xFF,
		0xFF, 0x10, 0x37, 0x5A,
		0x87, 0xBD, 0xFF, 0x9F,
		0x00
	}
};
#endif

static struct dsi_cmd_desc hitachi_power_on_set[] = {
	/* Display initial set */
	{DTYPE_DCS_WRITE1, 1, 0, 0, 20, sizeof(set_address_mode),
		set_address_mode},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(set_pixel_format),
		set_pixel_format},

	/* Sleep mode exit */
	{DTYPE_DCS_WRITE, 1, 0, 0, 70, sizeof(exit_sleep), exit_sleep},

	/* Manufacturer command protect off */
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(macp_off), macp_off},
#if defined(CONFIG_LGE_BACKLIGHT_CABC)
	/* Content adaptive backlight control */
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(backlight_ctrl1[1]),
		backlight_ctrl1[CABC_ON]},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(backlight_ctrl2[CABC_DEFUALT]),
		backlight_ctrl2[CABC_DEFUALT]},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(backlight_ctrl3[CABC_DEFUALT]),
		backlight_ctrl3[CABC_DEFUALT]},
#endif
	/* Manufacturer command protect on */
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(macp_on), macp_on},
	/* Display on */
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(display_on), display_on},
};

static struct dsi_cmd_desc hitachi_power_off_set[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(enter_sleep), enter_sleep}
};

#if defined (CONFIG_LGE_BACKLIGHT_CABC) &&\
		defined (CONFIG_LGE_BACKLIGHT_CABC_DEBUG)
void set_hitachi_cabc(int cabc_index)
{
	switch(cabc_index) {
	case 0: /* off */
	case 1: /* 10% */
	case 2: /* 20% */
	case 3: /* 30% */
	case 4: /* 40% */
	case 5: /* 50% */
		if (cabc_index == 0) { /* CABC OFF */
			hitachi_power_on_set[CABC_POWERON_OFFSET].payload =
						backlight_ctrl1[CABC_OFF];
		} else { /* CABC ON */
			hitachi_power_on_set[CABC_POWERON_OFFSET].payload =
						backlight_ctrl1[CABC_ON];
			hitachi_power_on_set[CABC_POWERON_OFFSET+1].payload =
						backlight_ctrl2[cabc_index];
			hitachi_power_on_set[CABC_POWERON_OFFSET+2].payload =
						backlight_ctrl3[cabc_index];
		}
		hitachi_cabc_index = cabc_index;
		break;
	default:
		printk("out of range cabc_index %d", cabc_index);
	}
	return;
}
EXPORT_SYMBOL(set_hitachi_cabc);

int get_hitachi_cabc(void)
{
	return hitachi_cabc_index;
}
EXPORT_SYMBOL(get_hitachi_cabc);

#endif
static struct msm_panel_common_pdata mipi_hitachi_pdata = {
	.backlight_level = mipi_hitachi_backlight_level,
	.power_on_set_1 = hitachi_power_on_set,
	.power_off_set_1 = hitachi_power_off_set,
	.power_on_set_size_1 = ARRAY_SIZE(hitachi_power_on_set),
	.power_off_set_size_1 = ARRAY_SIZE(hitachi_power_off_set),
};

static struct platform_device mipi_dsi_hitachi_panel_device = {
	.name = "mipi_hitachi",
	.id = 0,
	.dev = {
		.platform_data = &mipi_hitachi_pdata,
	}
};
#elif defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WXGA_PT)
static int mipi_lgit_backlight_level(int level, int max, int min)
{
	lm3530_lcd_backlight_set_level(level);

	return 0;
}


//LGE_CHANGE_S [hj.eum@lge.com]  2012_02_06, for making one source of DSV feature.
char lcd_mirror [2] = {0x36, 0x02};

// values of DSV setting START
static char panel_setting_1 [6] = {0xB0, 0x43, 0x00, 0x00, 0x00, 0x00};
static char panel_setting_2 [3] = {0xB3, 0x0A, 0x9F};

static char display_mode1 [6] = {0xB5, 0x50, 0x20, 0x40, 0x00, 0x20};
static char display_mode2 [8] = {0xB6, 0x00, 0x14, 0x0F, 0x16, 0x13, 0x05, 0x05};

static char p_gamma_r_setting[10] = {0xD0, 0x72, 0x15, 0x76, 0x00, 0x00, 0x00, 0x50, 0x30, 0x02};
static char n_gamma_r_setting[10] = {0xD1, 0x72, 0x15, 0x76, 0x00, 0x00, 0x00, 0x50, 0x30, 0x02};
static char p_gamma_g_setting[10] = {0xD2, 0x72, 0x15, 0x76, 0x00, 0x00, 0x00, 0x50, 0x30, 0x02};
static char n_gamma_g_setting[10] = {0xD3, 0x72, 0x15, 0x76, 0x00, 0x00, 0x00, 0x50, 0x30, 0x02};
static char p_gamma_b_setting[10] = {0xD4, 0x72, 0x15, 0x76, 0x00, 0x00, 0x00, 0x50, 0x30, 0x02};
static char n_gamma_b_setting[10] = {0xD5, 0x72, 0x15, 0x76, 0x00, 0x00, 0x00, 0x50, 0x30, 0x02};

//LGE_UPDATE_S hojin.ryu@lge.com 20120629 
#if defined(LGIT_IEF)
static char ief_on_set0[2] = {0xE0, 0x07};
static char ief_cabc_set[6] = {0xC8, 0x22, 0x22, 0x22, 0x33, 0x93};//MIE ON
static char ief_on_set4[4] = {0xE4, 0x02, 0x82, 0x82};
static char ief_on_set5[4] = {0xE5, 0x01, 0x82, 0x80};
static char ief_on_set6[4] = {0xE6, 0x04, 0x05, 0x07};

static char ief_off_set0[2] = {0xE0, 0x00};
static char ief_off_set4[4] = {0xE4, 0x00, 0x00, 0x00};
static char ief_off_set5[4] = {0xE5, 0x00, 0x00, 0x00};
static char ief_off_set6[4] = {0xE6, 0x00, 0x00, 0x00};

static char ief_set1[5] = {0xE1, 0x00, 0x00, 0x01, 0x01};
static char ief_set2[3] = {0xE2, 0x01, 0x0F};
static char ief_set3[6] = {0xE3, 0x00, 0x00, 0x31, 0x35, 0x00};
#endif
//LGE_UPDATE_E hojin.ryu@lge.com 20120629

static char osc_setting[4] =     {0xC0, 0x00, 0x0A, 0x10};
static char power_setting3[13] = {0xC3, 0x00, 0x88, 0x03, 0x20, 0x01, 0x57, 0x4F, 0x33,0x02,0x38,0x38,0x00};
static char power_setting4[6] =  {0xC4, 0x22, 0x24, 0x11, 0x11, 0x3D};
static char power_setting5[4] =  {0xC5, 0x3B, 0x3B, 0x03};

#if defined(CONFIG_LGE_BACKLIGHT_CABC)
static char cabc_set0[2] = {0x51, 0xFF};
static char cabc_set1[2] = {0x5E, 0x00}; // CABC MIN
static char cabc_set2[2] = {0x53, 0x2C};
static char cabc_set3[2] = {0x55, 0x02};
static char cabc_set4[6] = {0xC8, 0x22, 0x22, 0x22, 0x33, 0x80};//A-CABC applied
#endif

static char exit_sleep_power_control_2[2] =  {0xC2,0x06};
static char exit_sleep_power_control_3[2] =  {0xC2,0x0E};
static char otp_protection[3] =  {0xF1,0x10,0x00};
static char sleep_out_for_cabc[2] = {0x11,0x00};
static char gate_output_enabled_by_manual[2] = {0xC1,0x08};

static char display_on[2] =  {0x29,0x00};

static char display_off[2] = {0x28,0x00};

static char enter_sleep[2] = {0x10,0x00};

static char analog_boosting_power_control[2] = {0xC2,0x00};
static char enter_sleep_power_control_3[2] = {0xC2,0x01};
static char enter_sleep_power_control_2[2] = {0xC2,0x00};

static char deep_standby[2] = {0xC1,0x02};

/* initialize device */
static struct dsi_cmd_desc lgit_power_on_set_1[] = {
	// Display Initial Set
	
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(lcd_mirror ),lcd_mirror},

	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_1 ),panel_setting_1},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_2 ),panel_setting_2},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(display_mode1 ),display_mode1},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(display_mode2 ),display_mode2},

	// Gamma Set
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(p_gamma_r_setting),p_gamma_r_setting},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(n_gamma_r_setting),n_gamma_r_setting},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(p_gamma_g_setting),p_gamma_g_setting},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(n_gamma_g_setting),n_gamma_g_setting},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(p_gamma_b_setting),p_gamma_b_setting},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(n_gamma_b_setting),n_gamma_b_setting},
 
#if defined(LGIT_IEF)
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_on_set0),ief_on_set0},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_set1),ief_set1},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_set2),ief_set2},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_set3),ief_set3},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_cabc_set),ief_cabc_set},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_on_set4),ief_on_set4},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_on_set5),ief_on_set5},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_on_set6),ief_on_set6},
#endif

	// Power Supply Set
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(osc_setting   ),osc_setting   }, 
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(power_setting3),power_setting3}, 
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(power_setting4),power_setting4},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(power_setting5),power_setting5},
		
#if defined(CONFIG_LGE_BACKLIGHT_CABC)
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cabc_set0),cabc_set0},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cabc_set1),cabc_set1},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cabc_set2),cabc_set2},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cabc_set3),cabc_set3},
#if !defined(LGIT_IEF)
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cabc_set4),cabc_set4},
#endif
#endif
};
	
static struct dsi_cmd_desc lgit_power_on_set_2[] = {
	{DTYPE_GEN_LWRITE,  1, 0, 0, 10, sizeof(exit_sleep_power_control_2	),exit_sleep_power_control_2	},
	{DTYPE_GEN_LWRITE,  1, 0, 0, 1, sizeof(exit_sleep_power_control_3	),exit_sleep_power_control_3	},
};

static struct dsi_cmd_desc lgit_power_on_set_3[] = {
	// Power Supply Set
	{DTYPE_GEN_LWRITE,  1, 0, 0, 0, sizeof(otp_protection),otp_protection},
	{DTYPE_GEN_LWRITE,  1, 0, 0, 0, sizeof(sleep_out_for_cabc),sleep_out_for_cabc  },
	{DTYPE_GEN_LWRITE,  1, 0, 0, 0, sizeof(gate_output_enabled_by_manual),gate_output_enabled_by_manual},
	{DTYPE_DCS_WRITE,  1, 0, 0, 0, sizeof(display_on),display_on},
};

static struct dsi_cmd_desc lgit_power_off_set_1[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 20, sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 5, sizeof(enter_sleep), enter_sleep},
};

static struct dsi_cmd_desc lgit_power_off_set_2[] = {
	{DTYPE_GEN_LWRITE, 1, 0, 0, 10, sizeof(analog_boosting_power_control), analog_boosting_power_control},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 10, sizeof(enter_sleep_power_control_3), enter_sleep_power_control_3},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(enter_sleep_power_control_2), enter_sleep_power_control_2},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 10, sizeof(deep_standby), deep_standby}
};
// values of DSV setting END

// values of normal setting START(not DSV)

//LGE_UPDATE_S hojin.ryu@lge.com 20120629 IEF function On/Off sets are added for camera preview
#ifdef LGIT_IEF_SWITCH
static struct dsi_cmd_desc lgit_ief_off_set[] = {		
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_off_set6),ief_off_set6},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_off_set5),ief_off_set5},	
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_off_set4),ief_off_set4},
#if defined(CONFIG_LGE_BACKLIGHT_CABC)
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(cabc_set4),cabc_set4},
#endif
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_off_set0),ief_off_set0},
};

static struct dsi_cmd_desc lgit_ief_on_set[] = {
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_on_set0),ief_on_set0},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_cabc_set),ief_cabc_set},		
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_on_set4),ief_on_set4},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_on_set5),ief_on_set5},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(ief_on_set6),ief_on_set6},
};
#endif
//LGE_UPDATE_E hojin.ryu@lge.com 20120629

static struct msm_panel_common_pdata mipi_lgit_pdata = {
	.backlight_level = mipi_lgit_backlight_level,
	.power_on_set_1 = lgit_power_on_set_1,
	.power_on_set_2 = lgit_power_on_set_2,
	.power_on_set_3 = lgit_power_on_set_3,

	.power_on_set_ief = lgit_ief_on_set,
	.power_off_set_ief = lgit_ief_off_set,

	.power_on_set_size_1 = ARRAY_SIZE(lgit_power_on_set_1),
	.power_on_set_size_2 = ARRAY_SIZE(lgit_power_on_set_2),
	.power_on_set_size_3 = ARRAY_SIZE(lgit_power_on_set_3),

	.power_on_set_ief_size = ARRAY_SIZE(lgit_ief_on_set),
	.power_off_set_ief_size = ARRAY_SIZE(lgit_ief_off_set),

	.power_off_set_1 = lgit_power_off_set_1,
	.power_off_set_2 = lgit_power_off_set_2,
	.power_off_set_size_1 = ARRAY_SIZE(lgit_power_off_set_1),
	.power_off_set_size_2 =ARRAY_SIZE(lgit_power_off_set_2),
};
static struct platform_device mipi_dsi_lgit_panel_device = {
	.name = "mipi_lgit",
	.id = 0,
	.dev = {
		.platform_data = &mipi_lgit_pdata,
	}
};
//LGE_UPDATE_E hojin.ryu@lge.com 20120629
#elif defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_INVERSE_PT)

static int mipi_lgit_backlight_level(int level, int max, int min)
{
	lm3533_lcd_backlight_set_level(level);
	return 0;
}

static char exit_sleep_mode[2]={0x11,0x00};

#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_INVERSE_PT)
static char set_address_mode[2]=	{0x36,0x40};
#endif

static char panel_setting_0[2]=	{0xB0,0x04};
static char panel_setting_1[7]=	{0xB3,0x14,0x00,0x00,0x00,0x00,0x00};				
//static char panel_setting_2[4]={0xB4,0x0C,0x00,0x00};
//static char panel_setting_3[4]={0xB5,0x00,0x00,0x00};
static char panel_setting_4[3]={0xB6,0x3a,0xd3};
//static char panel_setting_5[2]={0xB7,0x00};
//static char panel_setting_6[3]={0xC0,0x00,0x00};

static char panel_setting_7[35]=	{0xC1,
					0x84,0x60,0x40,0x00,0x00, 
					0x00,0x00,0x00,0x00,0x0C,
					0x01,0x58,0x73,0xAE,0x31,
					0x20,0x06,0x00,0x00,0x00,
					0x00,0x00,0x00,0x10,0x10,
					0x10,0x10,0x00,0x00,0x00,
					0x22,0x02,0x02,0x00};

static char panel_setting_8[8]=		{0xC2,
					0x30,0xF7,0x80,0x0A,0x08,
					0x00,0x00};

//static char panel_setting_9[4]=	{0xC3,0x00,0x00,0x00};

static char panel_setting_10[23]=	{0xC4,
					0x70,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x11,
					0x03,0x00,0x00,0x00,0x00,
					0x00,0x00,0x00,0x00,0x00,	
					0x11,0x03};
					
static char panel_setting_11[41]=	{0xC6,
					0x06,0x6D,0x06,0x6D,0x06,
					0x6D,0x00,0x00,0x00,0x00,
					0x06,0x6D,0x06,0x6D,0x06,
					0x6D,0x15,0x19,0x07,0x00,
					0x01,0x06,0x6D,0x06,0x6D,
					0x06,0x6D,0x00,0x00,0x00,
					0x00,0x06,0x6D,0x06,0x6D,
					0x06,0x6D,0x15,0x19,0x07};
					
static char panel_setting_12[25]=	{0xC7,
					0x00,0x10,0x1A,0x1D,0x2D,
					0x45,0x41,0x54,0x5F,0x7B,
					0x7D,0x7F,0x00,0x10,0x1A,
					0x1D,0x2D,0x45,0x41,0x54,
					0x5F,0x7B,0x7D,0x7F };
					
static char panel_setting_13[25]=	{0xC8,
					0x00,0x10,0x1A,0x1D,0x2D,
					0x45,0x41,0x54,0x5F,0x7B,
					0x7D,0x7F,0x00,0x10,0x1A,
					0x1D,0x2D,0x45,0x41,0x54,
					0x5F,0x7B,0x7D,0x7F };
					
static char panel_setting_14[25]=	{0xC9,
					0x00,0x10,0x1A,0x1D,0x2D,
					0x45,0x41,0x54,0x5F,0x7B,
					0x7D,0x7F,0x00,0x10,0x1A,
					0x1D,0x2D,0x45,0x41,0x54,
					0x5F,0x7B,0x7D,0x7F };

//static char panel_setting_15[10]=	{0xCb,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0xc0};
					
					
static char panel_setting_16[2]=	{0xCc,0x09};


static char panel_setting_17[15]=	{0xd0,
					0x00,0x00,0x19,0x18,0x99,
					0x99,0x19,0x01,0x89,0x00,
					0x55,0x19,0x99,0x01};
				
					
//static char panel_setting_18[30]=	{0xd1,0x20,0x00,0x00,0x04,
//					0x08,0x0c,0x10,0x00,0x00,
//					0x00,0x00,0x00,0x3c,0x04,
//					0x20,0x00,0x00,0x04,0x08,
//					0x0c,0x10,0x00,0x00,0x3c,
//					0x06,0x40,0x00,0x32,0x31};
		
					
static char panel_setting_19[27]=	{0xd3,
					0x1B,0x33,0xBB,0xFF,0xF7,
					0x33,0x33,0x33,0x00,0x01,
					0x00,0xA0,0xD8,0xA0,0x0D,
					0x3B,0x33,0x44,0x22,0x70,
					0x02,0x3B,0x03,0x3D,0xBF,
					0x00};
					
//static char panel_setting_20[4]={0xd2,0x5c,0x00,0x00};

static char panel_setting_21[8]=	{0xd5,
					0x06,0x00,0x00,0x01,0x50,
					0x01,0x50};
//static char panel_setting_22[2]={0xd6,0x01};


//static char panel_setting_23[21]=	{0xd7,0x84,0xe0,0x7f,0xa8,
//					0xcd,0x38,0xfc,0xc1,0x83,
//					0xee,0x8f,0x1f,0x3c,0x10,
//					0xfa,0xc3,0x0f,0x04,0x41,
//					0x00};
					
//static char panel_setting_24[7]={0xd8,0x00,0x80,0x80,0xc0,0x42,0x21};
//static char panel_setting_25[3]={0xd9,0x00,0x00};
//static char panel_setting_26[3]={0xdd,0x10,0x8c};
//static char panel_setting_27[7]={0xde,0x00,0xff,0x07,0x10,0x00,0x78};


static char display_on[2] =  {0x29,0x00};

static char display_off[2] = {0x28,0x00};

static char enter_sleep_mode[2] = {0x10,0x00};

static char deep_standby_mode[2] = {0xB1,0x01};


/* initialize device */
static struct dsi_cmd_desc lgit_power_on_set[] = {
	// Display Initial Set

#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_INVERSE_PT)
	{DTYPE_DCS_WRITE1, 1, 0, 0, 20, sizeof(set_address_mode),set_address_mode},
#endif
	{DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(exit_sleep_mode), exit_sleep_mode},

	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_0 ),panel_setting_0},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_1 ),panel_setting_1},
	//{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_2 ),panel_setting_2},
	//{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_3 ),panel_setting_3},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_4 ),panel_setting_4},
	//{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_5 ),panel_setting_5},
	//{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_6 ),panel_setting_6},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_7 ),panel_setting_7},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_8 ),panel_setting_8},
	//{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_9 ),panel_setting_9},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_10 ),panel_setting_10},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_11 ),panel_setting_11},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_12 ),panel_setting_12},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_13 ),panel_setting_13},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_14 ),panel_setting_14},
	//{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_15 ),panel_setting_15},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_16 ),panel_setting_16},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_17 ),panel_setting_17},
	//{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_18 ),panel_setting_18},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_19 ),panel_setting_19},
	//{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_20 ),panel_setting_20},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_21 ),panel_setting_21},
	//{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_22 ),panel_setting_22},
	//{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_23 ),panel_setting_23},
	//{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_24 ),panel_setting_24},
	//{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_25 ),panel_setting_25},
	//{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_26 ),panel_setting_26},
	//{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(panel_setting_27 ),panel_setting_27},

	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(display_on ),display_on},
		
};

static struct dsi_cmd_desc lgit_power_off_set[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 20, sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 20, sizeof(enter_sleep_mode), enter_sleep_mode},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 20, sizeof(deep_standby_mode), deep_standby_mode}
};

// values of DSV setting END

// values of normal setting START(not DSV)


static struct msm_panel_common_pdata mipi_lgit_pdata = {
	.backlight_level = mipi_lgit_backlight_level,
	.power_on_set_1 = lgit_power_on_set,
	.power_on_set_size_1 = ARRAY_SIZE(lgit_power_on_set),
	.power_off_set_1 = lgit_power_off_set,
	.power_off_set_size_1 = ARRAY_SIZE(lgit_power_off_set),

};
static struct platform_device mipi_dsi_lgit_panel_device = {
	.name = "mipi_lgit",
	.id = 0,
	.dev = {
		.platform_data = &mipi_lgit_pdata,
	}
};
#endif

static struct platform_device *j1_panel_devices[] __initdata = {
#if defined(CONFIG_FB_MSM_MIPI_HITACHI_VIDEO_HD_PT)
	&mipi_dsi_hitachi_panel_device,
#elif defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_WXGA_PT) || defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_INVERSE_PT)
	&mipi_dsi_lgit_panel_device,
#endif
#ifdef CONFIG_LCD_KCAL
	&kcal_platrom_device,
#endif
};

void __init apq8064_init_fb(void)
{
	platform_device_register(&msm_fb_device);

#ifdef CONFIG_FB_MSM_WRITEBACK_MSM_PANEL
	platform_device_register(&wfd_panel_device);
	platform_device_register(&wfd_device);
#endif

	platform_add_devices(j1_panel_devices, ARRAY_SIZE(j1_panel_devices));

	msm_fb_register_device("mdp", &mdp_pdata);
	msm_fb_register_device("mipi_dsi", &mipi_dsi_pdata);

	platform_device_register(&hdmi_msm_device);
	msm_fb_register_device("dtv", &dtv_pdata);

}

#if defined(CONFIG_LGE_BACKLIGHT_CABC)
#define PWM_SIMPLE_EN 0xA0
#define PWM_BRIGHTNESS 0x20
#endif

struct backlight_platform_data {
   void (*platform_init)(void);
   int gpio;
   unsigned int mode;
   int max_current;
   int init_on_boot;
   int min_brightness;
   int max_brightness;
   int default_brightness;
   int factory_brightness;
};
//LGE_UPDATE_S hojin.ryu@lge.com Exponential BL setting 20120731
#if defined (CONFIG_LGE_BACKLIGHT_LM3530)
static struct backlight_platform_data lm3530_data = {

	.gpio = PM8921_GPIO_PM_TO_SYS(24),
#if defined(CONFIG_LGE_BACKLIGHT_CABC)
	.max_current = 0x15 | PWM_BRIGHTNESS,
#else
	.max_current = 0x15,
#endif
	.min_brightness = 0x36,
	.max_brightness = 0x7B,
	
};
//LGE_UPDATE_E hojin.ryu@lge.com Exponential BL setting 20120731
#elif defined(CONFIG_LGE_BACKLIGHT_LM3533)
static struct backlight_platform_data lm3533_data = {
	.gpio = PM8921_GPIO_PM_TO_SYS(24),
#if defined(CONFIG_LGE_BACKLIGHT_CABC)
	.max_current = 0x17 | PWM_SIMPLE_EN,
#else
	.max_current = 0x17,
#endif
	.min_brightness = 0x05,
	.max_brightness = 0xFF,
	.default_brightness = 0x9C,
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_INVERSE_PT)
	.factory_brightness = 0x78,
#else
	.factory_brightness = 0x45,
#endif
};
#endif
static struct i2c_board_info msm_i2c_backlight_info[] = {
	{
#if defined(CONFIG_LGE_BACKLIGHT_LM3530)
		I2C_BOARD_INFO("lm3530", 0x38),
		.platform_data = &lm3530_data,
#elif defined(CONFIG_LGE_BACKLIGHT_LM3533)
#if defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_PT) ||\
	defined(CONFIG_FB_MSM_MIPI_LGIT_VIDEO_FHD_INVERSE_PT)
// need to add lm3630 feature
		I2C_BOARD_INFO("lm3533", 0x38),
#else	
		I2C_BOARD_INFO("lm3533", 0x36),
#endif
		.platform_data = &lm3533_data,
#endif
	}
};

static struct i2c_registry apq8064_i2c_backlight_device[] __initdata = {

	{
	    I2C_SURF | I2C_FFA | I2C_RUMI | I2C_SIM | I2C_LIQUID | I2C_J1V,
		APQ_8064_GSBI1_QUP_I2C_BUS_ID,
		msm_i2c_backlight_info,
		ARRAY_SIZE(msm_i2c_backlight_info),
	},
};

void __init register_i2c_backlight_devices(void)
{
	u8 mach_mask = 0;
	int i;

	/* Build the matching 'supported_machs' bitmask */
	if (machine_is_apq8064_cdp())
		mach_mask = I2C_SURF;
	else if (machine_is_apq8064_mtp())
		mach_mask = I2C_FFA;
	else if (machine_is_apq8064_liquid())
		mach_mask = I2C_LIQUID;
	else if (machine_is_apq8064_rumi3())
		mach_mask = I2C_RUMI;
	else if (machine_is_apq8064_sim())
		mach_mask = I2C_SIM;
	else
		pr_err("unmatched machine ID in register_i2c_devices\n");	

	/* Run the array and install devices as appropriate */
	for (i = 0; i < ARRAY_SIZE(apq8064_i2c_backlight_device); ++i) {
		if (apq8064_i2c_backlight_device[i].machs & mach_mask)
			i2c_register_board_info(apq8064_i2c_backlight_device[i].bus,
						apq8064_i2c_backlight_device[i].info,
						apq8064_i2c_backlight_device[i].len);
	}
}
//LGE_UPDATE_E hojin.ryu@lge.com Backlight Pdata is migrated into Board-j1-display 20120413

#ifdef CONFIG_LGE_HIDDEN_RESET
int lge_get_fb_phys_info(unsigned long *start, unsigned long *size)
{
	if (!start || !size)
		return -1;
	*start = (unsigned long)msm_fb_resources[0].start;
	*size = (unsigned long)(LCD_RESOLUTION_X * LCD_RESOLUTION_Y * 4);
	return 0;
}

void *lge_get_hreset_fb_phys_addr(void)
{
	return (void *)0x88740000;
}
#endif
