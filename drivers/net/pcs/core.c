// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Sean Anderson <sean.anderson@seco.com>
 */

#include <linux/fwnode.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/pcs.h>
#include <linux/phylink.h>
#include <linux/property.h>

static LIST_HEAD(pcs_devices);
static DEFINE_MUTEX(pcs_mutex);

int pcs_component_bind(struct device *dev, struct device *master, void *data)
{
	return 0;
}

void pcs_component_unbind(struct device *dev, struct device *master,
			  void *data)
{
}

struct component_ops pcs_component_ops = {
	.bind = pcs_component_bind,
	.unbind = pcs_component_unbind,
};

/**
 * pcs_register() - register a new PCS
 * @pcs: the PCS to register
 *
 * Registers a new PCS which can be attached to a phylink.
 *
 * Return: 0 on success, or -errno on error
 */
int pcs_register(struct phylink_pcs *pcs)
{
	int ret;

	if (!pcs->dev || !pcs->ops)
		return -EINVAL;
	if (!pcs->ops->pcs_an_restart || !pcs->ops->pcs_config ||
	    !pcs->ops->pcs_get_state)
		return -EINVAL;

	ret = component_add(pcs->dev, &pcs_component_ops);
	if (ret)
		return ret;

	INIT_LIST_HEAD(&pcs->list);
	mutex_lock(&pcs_mutex);
	list_add(&pcs->list, &pcs_devices);
	mutex_unlock(&pcs_mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(pcs_register);

/**
 * pcs_unregister() - unregister a PCS
 * @pcs: a PCS previously registered with pcs_register()
 */
void pcs_unregister(struct phylink_pcs *pcs)
{
	component_del(pcs->dev, &pcs_component_ops);
	mutex_lock(&pcs_mutex);
	list_del(&pcs->list);
	mutex_unlock(&pcs_mutex);
}
EXPORT_SYMBOL_GPL(pcs_unregister);

static void devm_pcs_release(struct device *dev, void *res)
{
	pcs_unregister(*(struct phylink_pcs **)res);
}

/**
 * devm_pcs_register - resource managed pcs_register()
 * @dev: device that is registering this PCS
 * @pcs: the PCS to register
 *
 * Managed pcs_register(). For PCSs registered by this function,
 * pcs_unregister() is automatically called on driver detach. See
 * pcs_register() for more information.
 *
 * Return: 0 on success, or -errno on failure
 */
int devm_pcs_register(struct device *dev, struct phylink_pcs *pcs)
{
	struct phylink_pcs **pcsp;
	int ret;

	pcsp = devres_alloc(devm_pcs_release, sizeof(*pcsp),
			    GFP_KERNEL);
	if (!pcsp)
		return -ENOMEM;

	ret = pcs_register(pcs);
	if (ret) {
		devres_free(pcsp);
		return ret;
	}

	*pcsp = pcs;
	devres_add(dev, pcsp);

	return ret;
}
EXPORT_SYMBOL_GPL(devm_pcs_register);

/**
 * _pcs_get_by_provider() - Look up and request a PCS
 * @fwnode: The PCS's fwnode
 * @dev: The PCS's device
 *
 * Search PCSs registered with pcs_register() for one with a matching
 * fwnode or device. Either @fwnode or @dev may be %NULL if matching against a
 * fwnode or device is not desired (respectively).
 *
 * Once a PCS is found, perform common operations necessary when getting a PCS
 * (increment reference counts, etc).
 *
 * Return: @pcs, or an error pointer on failure. If both @fwnode and @dev are
 *         %NULL, returns %NULL to allow easier chaining.
 */
struct phylink_pcs *_pcs_get_by_provider(const struct fwnode_handle *fwnode,
					 const struct device *dev)
{
	struct phylink_pcs *pcs;

	if (!fwnode && !dev)
		return NULL;

	/*
	 * We need to hold this until we get to device_link_add. Otherwise,
	 * someone could unbind the PCS driver.
	 */
	mutex_lock(&pcs_mutex);
	list_for_each_entry(pcs, &pcs_devices, list) {
		if (dev && pcs->dev == dev)
			goto found;
		if (fwnode && pcs->dev->fwnode == fwnode)
			goto found;
	}
	pcs = ERR_PTR(-EPROBE_DEFER);

found:
	mutex_unlock(&pcs_mutex);
	pr_debug("looking for %pfwf or %s %s...%s found\n", fwnode,
		 dev ? dev_driver_string(dev) : "(null)",
		 dev ? dev_name(dev) : "(null)",
		 IS_ERR(pcs) ? " not" : "");
	if (IS_ERR(pcs))
		return pcs;

	get_device(pcs->dev);
	return pcs;
}
EXPORT_SYMBOL_GPL(_pcs_get_by_provider);

/**
 * _pcs_find_fwnode() - Find a PCS's fwnode
 * @mac_node: The fwnode referencing the PCS
 * @id: The name of the PCS to get. May be %NULL to get the first PCS.
 * @optional: Whether the PCS is optional
 *
 * Find a PCS's fwnode, as referenced by @mac_node. This fwnode can later be
 * used with pcs_get_by_provider() to get the actual PCS. pcs-names is used to
 * match @id, then the fwnode is found using pcs-handle.
 *
 * Return: %NULL if @optional is set and the PCS cannot be found. Otherwise,
 *         returns a PCS if found or an error pointer onm failure.
 */
struct fwnode_handle *_pcs_find_fwnode(const struct fwnode_handle *mac_node,
				       const char *id, bool optional)
{
	int index;
	struct fwnode_handle *pcs_fwnode;

	if (!mac_node)
		return optional ? NULL : ERR_PTR(-ENODEV);

	if (id)
		index = fwnode_property_match_string(mac_node,
						     "pcs-handle-names", id);
	else
		index = 0;

	if (index < 0) {
		if (optional && (index == -EINVAL || index == -ENODATA))
			return NULL;
		return ERR_PTR(index);
	}

	/* First try pcs-handle, and if that doesn't work fall back to the
	 * (legacy) pcsphy-handle.
	 */
	pcs_fwnode = fwnode_find_reference(mac_node, "pcs-handle", index);
	if (PTR_ERR(pcs_fwnode) == -ENOENT)
		pcs_fwnode = fwnode_find_reference(mac_node, "pcsphy-handle",
						   index);
	if (optional && !id && PTR_ERR(pcs_fwnode) == -ENOENT)
		return NULL;
	return pcs_fwnode;
}
EXPORT_SYMBOL_GPL(_pcs_find_fwnode);

/**
 * _pcs_get_by_fwnode() - Get a PCS from a fwnode property
 * @mac_node: The fwnode to get an associated PCS of
 * @id: The name of the PCS to get. May be %NULL to get the first PCS.
 * @optional: Whether the PCS is optional
 *
 * Find a PCS referenced by @mac_node and return a reference to it. Every call
 * to _pcs_get_by_fwnode() must be balanced with one to pcs_put().
 *
 * Return: a PCS if found, %NULL if not, or an error pointer on failure
 */
struct phylink_pcs *_pcs_get_by_fwnode(const struct fwnode_handle *mac_node,
				       const char *id, bool optional)
{
	struct fwnode_handle *pcs_fwnode;
	struct phylink_pcs *pcs;

	pcs_fwnode = _pcs_find_fwnode(mac_node, id, optional);
	if (IS_ERR(pcs_fwnode))
		return ERR_CAST(pcs_fwnode);

	pcs = pcs_get_by_provider_node(pcs_fwnode);
	fwnode_handle_put(pcs_fwnode);
	return pcs;
}
EXPORT_SYMBOL_GPL(_pcs_get_by_fwnode);

/**
 * pcs_put() - Release a previously-acquired PCS
 * @pcs: The PCS to put
 *
 * This frees resources associated with the PCS which were acquired when it was
 * gotten.
 */
void pcs_put(struct phylink_pcs *pcs)
{
	if (!pcs)
		return;

	put_device(pcs->dev);
}
EXPORT_SYMBOL_GPL(pcs_put);
