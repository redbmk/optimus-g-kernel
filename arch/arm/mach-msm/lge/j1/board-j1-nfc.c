#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <mach/board_lge.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/regulator/consumer.h>
#include "devices.h"
//#include CONFIG_BOARD_HEADER_FILE  

/* ehee.lee@lge.com */
#include "board-j1.h"
#include "board-8064.h"
/* ehee.lee@lge.com */

#if defined(CONFIG_LGE_NFC_PN544)
#include <linux/nfc/pn544_lge.h>

typedef void (gpio_i2c_init_func_t)(int bus_num);
/* ehee.lee@lge.com */

static struct pn544_i2c_platform_data pn544_i2c_platform_data[] = {
	{
		.ven_gpio 		= NFC_GPIO_VEN,
		.irq_gpio 	 	= NFC_GPIO_IRQ,
		.firm_gpio		= NFC_GPIO_FIRM,
	},
};

static struct i2c_board_info msm_i2c_nxp_nfc_info[] = {
	{
		I2C_BOARD_INFO("pn544", NFC_I2C_SLAVE_ADDR),
		.platform_data = &pn544_i2c_platform_data,
		.irq = MSM_GPIO_TO_INT(NFC_GPIO_IRQ),
	}
};

static struct  i2c_registry apq8064_i2c_devices __initdata = {
	//I2C_SURF | I2C_FFA | I2C_FLUID | I2C_RUMI,
	I2C_SURF | I2C_FFA | I2C_RUMI | I2C_SIM | I2C_LIQUID | I2C_J1V,
	APQ_8064_GSBI1_QUP_I2C_BUS_ID,
	msm_i2c_nxp_nfc_info,
	ARRAY_SIZE(msm_i2c_nxp_nfc_info),
};

static void __init lge_add_i2c_nfc_devices(void)
{
	i2c_register_board_info(apq8064_i2c_devices.bus,
			       apq8064_i2c_devices.info,
			       apq8064_i2c_devices.len);
}

void __init lge_add_nfc_devices(void)
{
	lge_add_i2c_nfc_devices();
}
#endif
