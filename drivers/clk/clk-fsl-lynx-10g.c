// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Sean Anderson <sean.anderson@seco.com>
 *
 * This file contains the implementation for the PLLs found on Lynx 10G phys.
 *
 * XXX: The VCO rate of the PLLs can exceed ~4GHz, which is the maximum rate
 * expressable in an unsigned long. To work around this, rates are specified in
 * kHz. This is as if there was a division by 1000 in the PLL.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/bitfield.h>
#include <linux/math64.h>
#include <linux/phy/lynx-10g.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/units.h>
#include <dt-bindings/clock/fsl,lynx-10g.h>

#define PLL_STRIDE	0x20
#define PLLa(a, off)	((a) * PLL_STRIDE + (off))
#define PLLaRSTCTL(a)	PLLa(a, 0x00)
#define PLLaCR0(a)	PLLa(a, 0x04)

#define PLLaRSTCTL_RSTREQ	BIT(31)
#define PLLaRSTCTL_RST_DONE	BIT(30)
#define PLLaRSTCTL_RST_ERR	BIT(29)
#define PLLaRSTCTL_PLLRST_B	BIT(7)
#define PLLaRSTCTL_SDRST_B	BIT(6)
#define PLLaRSTCTL_SDEN		BIT(5)

#define PLLaRSTCTL_ENABLE_SET	(PLLaRSTCTL_RST_DONE | PLLaRSTCTL_PLLRST_B | \
				 PLLaRSTCTL_SDRST_B | PLLaRSTCTL_SDEN)
#define PLLaRSTCTL_ENABLE_MASK	(PLLaRSTCTL_ENABLE_SET | PLLaRSTCTL_RST_ERR)

#define PLLaCR0_POFF		BIT(31)
#define PLLaCR0_RFCLK_SEL	GENMASK(30, 28)
#define PLLaCR0_PLL_LCK		BIT(23)
#define PLLaCR0_FRATE_SEL	GENMASK(19, 16)
#define PLLaCR0_DLYDIV_SEL	GENMASK(1, 0)

#define PLLaCR0_DLYDIV_SEL_16		0b01

/**
 * struct lynx_clk - Driver data for the PLLs
 * @pll: The PLL clock
 * @ex_dly: The "PLLa_ex_dly_clk" clock
 * @dev: The serdes device
 * @regmap: Our registers
 * @idx: Which PLL this clock is for
 */
struct lynx_clk {
	struct clk_hw pll, ex_dly;
	struct device *dev;
	struct regmap *regmap;
	unsigned int idx;
};

static u32 lynx_read(struct lynx_clk *clk, u32 reg)
{
	unsigned int ret = 0;

	WARN_ON_ONCE(regmap_read(clk->regmap, reg, &ret));
	return ret;
}

static void lynx_write(struct lynx_clk *clk, u32 val, u32 reg)
{
	WARN_ON_ONCE(regmap_write(clk->regmap, reg, val));
}

static struct lynx_clk *lynx_pll_to_clk(struct clk_hw *hw)
{
	return container_of(hw, struct lynx_clk, pll);
}

static struct lynx_clk *lynx_ex_dly_to_clk(struct clk_hw *hw)
{
	return container_of(hw, struct lynx_clk, ex_dly);
}

static void lynx_pll_stop(struct lynx_clk *clk)
{
	u32 rstctl;

	rstctl = lynx_read(clk, PLLaRSTCTL(clk->idx));
	rstctl &= ~PLLaRSTCTL_SDRST_B;
	lynx_write(clk, rstctl, PLLaRSTCTL(clk->idx));

	ndelay(50);

	rstctl = lynx_read(clk, PLLaRSTCTL(clk->idx));
	rstctl &= ~(PLLaRSTCTL_SDEN | PLLaRSTCTL_PLLRST_B);
	lynx_write(clk, rstctl, PLLaRSTCTL(clk->idx));

	ndelay(100);
}

static void lynx_pll_disable(struct clk_hw *hw)
{
	struct lynx_clk *clk = lynx_pll_to_clk(hw);
	u32 cr0;

	dev_dbg(clk->dev, "disable pll%d\n", clk->idx);

	lynx_pll_stop(clk);

	cr0 = lynx_read(clk, PLLaCR0(clk->idx));
	cr0 |= PLLaCR0_POFF;
	lynx_write(clk, cr0, PLLaCR0(clk->idx));
}

static int lynx_pll_reset(struct lynx_clk *clk)
{
	int ret;
	u32 rstctl = lynx_read(clk, PLLaRSTCTL(clk->idx));

	rstctl |= PLLaRSTCTL_RSTREQ;
	lynx_write(clk, rstctl, PLLaRSTCTL(clk->idx));
	ret = read_poll_timeout(lynx_read, rstctl,
				rstctl & (PLLaRSTCTL_RST_DONE | PLLaRSTCTL_RST_ERR),
				100, 5000, true, clk, PLLaRSTCTL(clk->idx));
	if (rstctl & PLLaRSTCTL_RST_ERR)
		ret = -EIO;
	if (ret) {
		dev_err(clk->dev, "pll%d reset failed\n", clk->idx);
		return ret;
	}

	rstctl |= PLLaRSTCTL_SDEN | PLLaRSTCTL_PLLRST_B | PLLaRSTCTL_SDRST_B;
	lynx_write(clk, rstctl, PLLaRSTCTL(clk->idx));
	return 0;
}

static int lynx_pll_prepare(struct clk_hw *hw)
{
	struct lynx_clk *clk = lynx_pll_to_clk(hw);
	u32 rstctl = lynx_read(clk, PLLaRSTCTL(clk->idx));
	u32 cr0 = lynx_read(clk, PLLaCR0(clk->idx));

	/*
	 * "Enabling" the PLL involves resetting it (and all attached lanes).
	 * Avoid doing this if we are already enabled.
	 */
	if (!(cr0 & PLLaCR0_POFF) &&
	    (rstctl & PLLaRSTCTL_ENABLE_MASK) == PLLaRSTCTL_ENABLE_SET) {
		dev_dbg(clk->dev, "pll%d already prepared\n", clk->idx);
		return 0;
	}

	dev_dbg(clk->dev, "prepare pll%d\n", clk->idx);

	cr0 &= ~PLLaCR0_POFF;
	lynx_write(clk, cr0, PLLaCR0(clk->idx));

	return lynx_pll_reset(clk);
}

static int lynx_pll_is_enabled(struct clk_hw *hw)
{
	struct lynx_clk *clk = lynx_pll_to_clk(hw);
	u32 cr0 = lynx_read(clk, PLLaCR0(clk->idx));
	bool enabled = !(cr0 & PLLaCR0_POFF);

	dev_dbg(clk->dev, "pll%d %s enabled\n", clk->idx,
		enabled ? "is" : "is not");

	return enabled;
}

static const unsigned long rfclk_sel_map[8] = {
	[0b000] = 100000000,
	[0b001] = 125000000,
	[0b010] = 156250000,
	[0b011] = 150000000,
};

/**
 * lynx_rfclk_to_sel() - Convert a reference clock rate to a selector
 * @rate: The reference clock rate
 *
 * To allow for some variation in the reference clock rate, up to 100ppm of
 * error is allowed.
 *
 * Return: An appropriate selector for @rate, or -%EINVAL.
 */
static int lynx_rfclk_to_sel(unsigned long rate)
{
	int ret;

	for (ret = 0; ret < ARRAY_SIZE(rfclk_sel_map); ret++) {
		unsigned long rfclk_rate = rfclk_sel_map[ret];
		/* Allow an error of 100ppm */
		unsigned long error = rfclk_rate / 10000;

		if (abs(rate - rfclk_rate) < error)
			return ret;
	}

	return -EINVAL;
}

static const unsigned long frate_sel_map[16] = {
	[0b0000] = 5000000,
	[0b0101] = 3750000,
	[0b0110] = 5156250,
	[0b0111] = 4000000,
	[0b1001] = 3125000,
	[0b1010] = 3000000,
};

/**
 * lynx_frate_to_sel() - Convert a VCO clock rate to a selector
 * @rate_khz: The VCO frequency, in kHz
 *
 * Return: An appropriate selector for @rate_khz, or -%EINVAL.
 */
static int lynx_frate_to_sel(unsigned long rate_khz)
{
	int ret;

	for (ret = 0; ret < ARRAY_SIZE(frate_sel_map); ret++)
		if (frate_sel_map[ret] == rate_khz)
			return ret;

	return -EINVAL;
}

static u32 lynx_pll_ratio(u32 frate_sel, u32 rfclk_sel)
{
	u64 frate;
	u32 rfclk, error, ratio;

	frate = frate_sel_map[frate_sel] * (u64)HZ_PER_KHZ;
	rfclk = rfclk_sel_map[rfclk_sel];

	if (!frate || !rfclk)
		return 0;

	ratio = div_u64_rem(frate, rfclk, &error);
	if (!error)
		return ratio;
	return 0;
}

static unsigned long lynx_pll_recalc_rate(struct clk_hw *hw,
					  unsigned long parent_rate)
{
	struct lynx_clk *clk = lynx_pll_to_clk(hw);
	u32 cr0 = lynx_read(clk, PLLaCR0(clk->idx));
	u32 frate_sel = FIELD_GET(PLLaCR0_FRATE_SEL, cr0);
	u32 rfclk_sel = FIELD_GET(PLLaCR0_RFCLK_SEL, cr0);
	u32 ratio = lynx_pll_ratio(frate_sel, rfclk_sel);
	unsigned long ret;

	/* Ensure that the parent matches our rfclk selector */
	if (rfclk_sel == lynx_rfclk_to_sel(parent_rate))
		ret = mult_frac(parent_rate, ratio, HZ_PER_KHZ);
	else
		ret = 0;

	dev_dbg(clk->dev, "recalc pll%d new=%llu parent=%lu\n", clk->idx,
		(u64)ret * HZ_PER_KHZ, parent_rate);
	return ret;
}

static int lynx_pll_determine_rate(struct clk_hw *hw,
				   struct clk_rate_request *req)
{
	int frate_sel, rfclk_sel;
	struct lynx_clk *clk = lynx_pll_to_clk(hw);
	u32 ratio;

	dev_dbg(clk->dev, "round pll%d new=%llu parent=%lu\n", clk->idx,
		(u64)req->rate * HZ_PER_KHZ, req->best_parent_rate);

	frate_sel = lynx_frate_to_sel(req->rate);
	if (frate_sel < 0)
		return frate_sel;

	/* Try the current parent rate */
	rfclk_sel = lynx_rfclk_to_sel(req->best_parent_rate);
	if (rfclk_sel >= 0) {
		ratio = lynx_pll_ratio(frate_sel, rfclk_sel);
		if (ratio) {
			req->rate = mult_frac(req->best_parent_rate, ratio,
					      HZ_PER_KHZ);
			return 0;
		}
	}

	/* Try all possible parent rates */
	for (rfclk_sel = 0;
	     rfclk_sel < ARRAY_SIZE(rfclk_sel_map);
	     rfclk_sel++) {
		unsigned long new_parent_rate;

		ratio = lynx_pll_ratio(frate_sel, rfclk_sel);
		if (!ratio)
			continue;

		/* Ensure the reference clock can produce this rate */
		new_parent_rate = rfclk_sel_map[rfclk_sel];
		new_parent_rate = clk_hw_round_rate(req->best_parent_hw,
						    new_parent_rate);
		if (rfclk_sel != lynx_rfclk_to_sel(new_parent_rate))
			continue;

		req->rate = mult_frac(new_parent_rate, ratio, HZ_PER_KHZ);
		req->best_parent_rate = new_parent_rate;
		return 0;
	}

	return -EINVAL;
}

static int lynx_pll_set_rate(struct clk_hw *hw, unsigned long rate_khz,
			     unsigned long parent_rate)
{
	int frate_sel, rfclk_sel;
	struct lynx_clk *clk = lynx_pll_to_clk(hw);
	u32 ratio, cr0 = lynx_read(clk, PLLaCR0(clk->idx));

	dev_dbg(clk->dev, "set rate pll%d new=%llu parent=%lu\n", clk->idx,
		(u64)rate_khz * HZ_PER_KHZ, parent_rate);

	frate_sel = lynx_frate_to_sel(rate_khz);
	if (frate_sel < 0)
		return frate_sel;

	rfclk_sel = lynx_rfclk_to_sel(parent_rate);
	if (rfclk_sel < 0)
		return rfclk_sel;

	ratio = lynx_pll_ratio(frate_sel, rfclk_sel);
	if (!ratio)
		return -EINVAL;

	lynx_pll_stop(clk);
	cr0 &= ~(PLLaCR0_RFCLK_SEL | PLLaCR0_FRATE_SEL);
	cr0 |= FIELD_PREP(PLLaCR0_RFCLK_SEL, rfclk_sel);
	cr0 |= FIELD_PREP(PLLaCR0_FRATE_SEL, frate_sel);
	lynx_write(clk, cr0, PLLaCR0(clk->idx));
	/* Don't bother resetting if it's off */
	if (cr0 & PLLaCR0_POFF)
		return 0;
	return lynx_pll_reset(clk);
}

static const struct clk_ops lynx_pll_clk_ops = {
	.prepare = lynx_pll_prepare,
	.disable = lynx_pll_disable,
	.is_enabled = lynx_pll_is_enabled,
	.recalc_rate = lynx_pll_recalc_rate,
	.determine_rate = lynx_pll_determine_rate,
	.set_rate = lynx_pll_set_rate,
};

static void lynx_ex_dly_disable(struct clk_hw *hw)
{
	struct lynx_clk *clk = lynx_ex_dly_to_clk(hw);
	u32 cr0 = lynx_read(clk, PLLaCR0(clk->idx));

	cr0 &= ~PLLaCR0_DLYDIV_SEL;
	lynx_write(clk, PLLaCR0(clk->idx), cr0);
}

static int lynx_ex_dly_enable(struct clk_hw *hw)
{
	struct lynx_clk *clk = lynx_ex_dly_to_clk(hw);
	u32 cr0 = lynx_read(clk, PLLaCR0(clk->idx));

	cr0 &= ~PLLaCR0_DLYDIV_SEL;
	cr0 |= FIELD_PREP(PLLaCR0_DLYDIV_SEL, PLLaCR0_DLYDIV_SEL_16);
	lynx_write(clk, PLLaCR0(clk->idx), cr0);
	return 0;
}

static int lynx_ex_dly_is_enabled(struct clk_hw *hw)
{
	struct lynx_clk *clk = lynx_ex_dly_to_clk(hw);

	return lynx_read(clk, PLLaCR0(clk->idx)) & PLLaCR0_DLYDIV_SEL;
}

static unsigned long lynx_ex_dly_recalc_rate(struct clk_hw *hw,
					     unsigned long parent_rate)
{
	return parent_rate / 16;
}

static const struct clk_ops lynx_ex_dly_clk_ops = {
	.enable = lynx_ex_dly_enable,
	.disable = lynx_ex_dly_disable,
	.is_enabled = lynx_ex_dly_is_enabled,
	.recalc_rate = lynx_ex_dly_recalc_rate,
};

static int lynx_clk_init(struct clk_hw_onecell_data *hw_data,
			 struct device *dev, struct regmap *regmap,
			 unsigned int index)
{
	const struct clk_hw *ex_dly_parents;
	struct clk_parent_data pll_parents[1] = { };
	struct clk_init_data pll_init = {
		.ops = &lynx_pll_clk_ops,
		.parent_data = pll_parents,
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
			 CLK_OPS_PARENT_ENABLE,
	};
	struct clk_init_data ex_dly_init = {
		.ops = &lynx_ex_dly_clk_ops,
		.parent_hws = &ex_dly_parents,
		.num_parents = 1,
	};
	struct lynx_clk *clk;
	int ret;

	clk = devm_kzalloc(dev, sizeof(*clk), GFP_KERNEL);
	if (!clk)
		return -ENOMEM;

	clk->dev = dev;
	clk->regmap = regmap;
	clk->idx = index;

	pll_parents[0].fw_name = kasprintf(GFP_KERNEL, "ref%d", index);
	pll_init.name = kasprintf(GFP_KERNEL, "%s.pll%d_khz", dev_name(dev),
				  index);
	ex_dly_init.name = kasprintf(GFP_KERNEL, "%s.pll%d_ex_dly_khz",
				     dev_name(dev), index);
	if (!pll_parents[0].fw_name || !pll_init.name || !ex_dly_init.name) {
		ret = -ENOMEM;
		goto out;
	}

	clk->pll.init = &pll_init;
	ret = devm_clk_hw_register(dev, &clk->pll);
	if (ret) {
		dev_err_probe(dev, ret, "could not register %s\n",
			      pll_init.name);
		goto out;
	}

	ex_dly_parents = &clk->pll;
	clk->ex_dly.init = &ex_dly_init;
	ret = devm_clk_hw_register(dev, &clk->ex_dly);
	if (ret)
		dev_err_probe(dev, ret, "could not register %s\n",
			      ex_dly_init.name);

	hw_data->hws[LYNX10G_PLLa(index)] = &clk->pll;
	hw_data->hws[LYNX10G_PLLa_EX_DLY(index)] = &clk->ex_dly;

out:
	kfree(pll_parents[0].fw_name);
	kfree(pll_init.name);
	kfree(ex_dly_init.name);
	return ret;
}

#define NUM_PLLS 2
#define NUM_CLKS (NUM_PLLS * LYNX10G_CLKS_PER_PLL)

int lynx_clks_init(struct device *dev, struct regmap *regmap,
		   struct clk *plls[2], struct clk *ex_dlys[2])
{
	int ret, i;
	struct clk_hw_onecell_data *hw_data;

	hw_data = devm_kzalloc(dev, struct_size(hw_data, hws, NUM_CLKS),
			       GFP_KERNEL);
	if (!hw_data)
		return -ENOMEM;
	hw_data->num = NUM_CLKS;

	for (i = 0; i < NUM_PLLS; i++) {
		ret = lynx_clk_init(hw_data, dev, regmap, i);
		if (ret)
			return ret;

		plls[i] = devm_clk_hw_get_clk(dev,
					      hw_data->hws[LYNX10G_PLLa(i)],
					      NULL);
		if (IS_ERR(plls[i]))
			return PTR_ERR(plls[i]);

		ex_dlys[i] = devm_clk_hw_get_clk(dev,
						 hw_data->hws[LYNX10G_PLLa_EX_DLY(i)],
						 NULL);
		if (IS_ERR(ex_dlys[i]))
			return PTR_ERR(plls[i]);
	}

	ret = devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get, hw_data);
	if (ret)
		dev_err_probe(dev, ret, "could not register clock provider\n");

	return ret;
}
EXPORT_SYMBOL_GPL(lynx_clks_init);

MODULE_AUTHOR("Sean Anderson <sean.anderson@seco.com>");
MODULE_DESCRIPTION("Lynx 10G PLL driver");
MODULE_LICENSE("GPL");
