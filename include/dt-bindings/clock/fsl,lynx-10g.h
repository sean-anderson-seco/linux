/* SPDX-License-Identifier: GPL-2.0-only OR BSD-2-Clause */
/*
 * Copyright (C) 2022 Sean Anderson <sean.anderson@seco.com>
 */

#ifndef __DT_BINDINGS_CLK_LYNX_10G_H
#define __DT_BINDINGS_CLK_LYNX_10G_H

#define LYNX10G_CLKS_PER_PLL 2

#define LYNX10G_PLLa(a)		((a) * LYNX10G_CLKS_PER_PLL)
#define LYNX10G_PLLa_EX_DLY(a)	((a) * LYNX10G_CLKS_PER_PLL + 1)

#endif /* __DT_BINDINGS_CLK_LYNX_10G_H */
