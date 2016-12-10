/*
 * include/plat/mphy.h
 *
 * Copyright (C) 2012 FriendlyARM (www.arm9.net)
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __PLAT_MPHY_H__
#define __PLAT_MPHY_H__


/* command list */
#define PHY_CMD_EHCI		1
#define PHY_CMD_SDHCI		2
#define PHY_CMD_OHCI	    3

/* PHY init function */
//这个函数在drivers/mtd/nand/s5p_nand_mlc.fo中定义，会用到对gpio虚拟地址的直接操作，由于我用的这个kernel与
//友善之臂的gpio虚拟地址不一样，所以不要使用！！！否则会导致系统崩溃。用这个头文件只是为了使用上面的两个宏定义，在SDHCI也可能会用到，先留着。
///extern int s5p_phy_init_ext(unsigned int cmd, unsigned long arg, void *p);


#endif	// __PLAT_MPHY_H__

