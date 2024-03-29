/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#ifndef _ARCH_ARM_MACH_MSM_MDM_PERIPHERAL_H
#define _ARCH_ARM_MACH_MSM_MDM_PERIPHERAL_H_

#include <linux/usb.h>

extern void peripheral_connect(void);
extern void peripheral_disconnect(void);

extern void dbg_log_event(struct urb *urb, char * event,unsigned extra);

#endif
