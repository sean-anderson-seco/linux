/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 Sean Anderson <sean.anderson@seco.com>
 */

#ifndef LYNX_10G
#define LYNX_10G

struct clk;
struct device;
struct regmap;

int lynx_clks_init(struct device *dev, struct regmap *regmap,
		   struct clk *plls[2], struct clk *ex_dlys[2]);

#endif /* LYNX 10G */
