/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 Sean Anderson <sean.anderson@seco.com>
 */

#ifndef _PCS_H
#define _PCS_H

#include <linux/property.h>
#include <linux/component.h>

struct phylink_pcs;

int pcs_register(struct phylink_pcs *pcs);
void pcs_unregister(struct phylink_pcs *pcs);
int devm_pcs_register(struct device *dev, struct phylink_pcs *pcs);
struct phylink_pcs *_pcs_get_by_provider(const struct fwnode_handle *fwnode,
					 const struct device *dev);
struct fwnode_handle *_pcs_find_fwnode(const struct fwnode_handle *mac_node,
				       const char *id, bool optional);
struct phylink_pcs *_pcs_get_by_fwnode(const struct fwnode_handle *mac_node,
				       const char *id, bool optional);
void pcs_put(struct phylink_pcs *pcs);

/* Various wrapper functions */

static inline struct phylink_pcs
*pcs_get_by_provider_node(const struct fwnode_handle *fwnode)
{
	return _pcs_get_by_provider(fwnode, NULL);
}

static inline struct phylink_pcs
*pcs_get_by_provider_dev(const struct device *dev)
{
	return _pcs_get_by_provider(NULL, dev);
}

static inline struct fwnode_handle
*pcs_find_fwnode_by_fwnode(const struct fwnode_handle *fwnode, const char *id)
{
	return _pcs_find_fwnode(fwnode, id, false);
}

static inline struct fwnode_handle
*pcs_find_fwnode_by_fwnode_optional(const struct fwnode_handle *fwnode,
				    const char *id)
{
	return _pcs_find_fwnode(fwnode, id, true);
}

static inline struct fwnode_handle *pcs_find_fwnode(struct device *dev,
						    const char *id)
{
	return _pcs_find_fwnode(dev_fwnode(dev), id, false);
}

static inline struct fwnode_handle *pcs_find_fwnode_optional(struct device *dev,
							     const char *id)
{
	return _pcs_find_fwnode(dev_fwnode(dev), id, true);
}

static inline struct fwnode_handle
*pcs_match(struct device *dev, const char *id,
	   struct component_match **matchptr)
{
	struct fwnode_handle *fwnode = pcs_find_fwnode(dev, id);

	if (!IS_ERR(fwnode))
		component_match_add_fwnode(dev, matchptr, fwnode);
	return fwnode;
}

static inline struct fwnode_handle
*pcs_match_optional(struct device *dev, const char *id,
		    struct component_match **matchptr)
{
	struct fwnode_handle *fwnode = pcs_find_fwnode_optional(dev, id);

	if (!IS_ERR_OR_NULL(fwnode))
		component_match_add_fwnode(dev, matchptr, fwnode);
	return fwnode;
}

static inline struct phylink_pcs
*pcs_get_by_fwnode(const struct fwnode_handle *fwnode, const char *id)
{
	return _pcs_get_by_fwnode(fwnode, id, false);
}

static inline struct phylink_pcs
*pcs_get_by_fwnode_optional(const struct fwnode_handle *fwnode, const char *id)
{
	return _pcs_get_by_fwnode(fwnode, id, true);
}

static inline struct phylink_pcs *pcs_get(struct device *dev, const char *id)
{
	return _pcs_get_by_fwnode(dev_fwnode(dev), id, false);
}

static inline struct phylink_pcs *pcs_get_optional(struct device *dev,
						   const char *id)
{
	return _pcs_get_by_fwnode(dev_fwnode(dev), id, true);
}

#endif /* PCS_H */
