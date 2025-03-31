.. SPDX-License-Identifier: GPL-2.0

=============
PCS Subsystem
=============

The PCS (Physical Coding Sublayer) subsystem handles the registration and lookup
of PCS devices. These devices contain the upper sublayers of the Ethernet
physical layer, generally handling framing, scrambling, and encoding tasks. PCS
devices may also include PMA (Physical Medium Attachment) components. PCS
devices transfer data between the Link-layer MAC device, and the rest of the
physical layer, typically via a serdes. The output of the serdes may be
connected more-or-less directly to the medium when using fiber-optic or
backplane connections (1000BASE-SX, 1000BASE-KX, etc). It may also communicate
with a separate PHY (such as over SGMII) which handles the connection to the
medium (such as 1000BASE-T).

Looking up PCS Devices
----------------------

There are generally two ways to look up a PCS device. If the PCS device is
internal to a larger device (such as a MAC or switch), and it does not share an
implementation with an existing PCS, then it does not need to be registered with
the PCS subsystem. Instead, you can populate a :c:type:`phylink_pcs`
in your probe function. Otherwise, you must look up the PCS.

If your device has a :c:type:`fwnode_handle`, you can add a PCS using the
``pcs-handle`` property::

    ethernet-controller {
        // ...
        pcs-handle = <&pcs>;
        pcs-handle-names = "internal";
    };

Then, during your probe function, you can get the PCS using :c:func:`pcs_get`::

    mac->pcs = pcs_get(dev, "internal");
    if (IS_ERR(mac->pcs)) {
        err = PTR_ERR(mac->pcs);
        return dev_err_probe(dev, "Could not get PCS\n");
    }

If your device doesn't have a :c:type:`fwnode_handle`, you can get the PCS
based on the providing device using :c:func:`pcs_get_by_dev`. Typically, you
will create the device and bind your PCS driver to it before calling this
function. This allows reuse of an existing PCS driver.

Once you are done using the PCS, you must call :c:func:`pcs_put`.

Using PCS Devices
-----------------

To select the PCS from a MAC driver, implement the ``mac_select_pcs`` callback
of :c:type:`phylink_mac_ops`. In this example, the PCS is selected for SGMII
and 1000BASE-X, and deselected for other interfaces::

    static struct phylink_pcs *mac_select_pcs(struct phylink_config *config,
                                              phy_interface_t iface)
    {
        struct mac *mac = config_to_mac(config);

        switch (iface) {
        case PHY_INTERFACE_MODE_SGMII:
        case PHY_INTERFACE_MODE_1000BASEX:
            return mac->pcs;
        default:
            return NULL;
        }
    }

To do the same from a DSA driver, implement the ``phylink_mac_select_pcs``
callback of :c:type:`dsa_switch_ops`.

Writing PCS Drivers
-------------------

To write a PCS driver, first implement :c:type:`phylink_pcs_ops`. Then,
register your PCS in your probe function using :c:func:`pcs_register`. You must
call :c:func:`pcs_unregister` from your remove function. You can avoid this step
by registering with :c:func:`devm_pcs_unregister`.

Normally, :ref:`device links <device_link>` will prevent improper ordering of
device unbinding/removal. However, if your PCS device can be a child of its
consumers (such as if it lives on an MDIO bus created by the MAC which uses the
PCS), then no device link will be created. This is because children must be
probed/removed after their parents, but a device link implies that the consumer
must be probed after the provider. This contradiction is generally resolved by
having the consumer probe the provider at an appropriate point in the consumer's
probe function. However, the ``unbind`` device attribute can let userspace
unbind the provider directly, bypassing this usual process. Therefore, PCS
drivers in this situation must set ``suppress_bind_attrs`` in their
:c:type:`device_driver`.


API Reference
-------------

.. kernel-doc:: include/linux/phylink.h
   :identifiers: phylink_pcs phylink_pcs_ops

.. kernel-doc:: include/linux/pcs.h
   :internal:

.. kernel-doc:: drivers/net/pcs/core.c
   :export:

.. kernel-doc:: drivers/net/pcs/core.c
   :internal:
