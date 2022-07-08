/* SPDX-License-Identifier: (GPL-2.0+ OR BSD-3-Clause) */
/* Copyright 2020 NXP
 * Lynx PCS helpers
 */

#ifndef __LINUX_PCS_LYNX_H
#define __LINUX_PCS_LYNX_H

struct device;
struct mii_bus;

struct device *lynx_pcs_create_on_bus(struct mii_bus *bus, int addr);

#endif /* __LINUX_PCS_LYNX_H */
