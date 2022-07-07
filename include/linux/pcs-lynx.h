/* SPDX-License-Identifier: (GPL-2.0+ OR BSD-3-Clause) */
/* Copyright 2020 NXP
 * Lynx PCS helpers
 */

#ifndef __LINUX_PCS_LYNX_H
#define __LINUX_PCS_LYNX_H

struct device;
struct mii_bus;
struct phylink_pcs;

struct mdio_device *lynx_get_mdio_device(struct phylink_pcs *pcs);

struct phylink_pcs *lynx_pcs_create(struct mdio_device *mdio);
struct device *lynx_pcs_create_on_bus(struct mii_bus *bus, int addr);

void lynx_pcs_destroy(struct phylink_pcs *pcs);

#endif /* __LINUX_PCS_LYNX_H */
