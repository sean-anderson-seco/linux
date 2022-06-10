.. SPDX-License-Identifier: GPL-2.0

===========================
Lynx 10G Phy (QorIQ SerDes)
===========================

Using this phy
--------------

:c:func:`phy_get` just gets (or creates) a new :c:type:`phy` with the lanes
described in the phandle. :c:func:`phy_init` is what actually reserves the
lanes for use. Unlike some other drivers, when the phy is created, there is no
default protocol. :c:func:`phy_set_mode <phy_set_mode_ext>` must be called in
order to set the protocol.

Supporting SoCs
---------------

Each new SoC needs a :c:type:`struct lynx_conf <lynx_conf>`, containing the
number of lanes in each device, the endianness of the device, and the helper
functions to use when selecting protocol controllers. For example, the
configuration for the LS1046A is::

    static const struct lynx_cfg ls1046a_cfg = {
        .lanes = 4,
        .endian = REGMAP_ENDIAN_BIG,
        .mode_conflict = lynx_ls_mode_conflict,
        .mode_apply = lynx_ls_mode_apply,
        .mode_init = lynx_ls_mode_init,
    };

The ``mode_`` functions will generally be common to all SoCs in a series (e.g.
all Layerscape SoCs or all T-series SoCs).

In addition, you will need to add a device node as documented in
``Documentation/devicetree/bindings/phy/fsl,lynx-10g.yaml``. This lets the
driver know which lanes are available to configure.

Supporting Protocols
--------------------

Each protocol is a combination of values which must be programmed into the lane
registers. To add a new protocol, first add it to :c:type:`enum lynx_protocol
<lynx_protocol>`. Add a new entry to ``lynx_proto_params``, and populate the
appropriate fields. Modify ``lynx_lookup_proto`` to map the :c:type:`enum
phy_mode <phy_mode>` to :c:type:`enum lynx_protocol <lynx_protocol>`. Finally,
update the ``mode_conflict``, ``mode_apply``, and ``mode_init`` helpers to
support your protocol.

You may need to modify :c:func:`lynx_set_mode` in order to support your
protocol. This can happen when you have added members to :c:type:`struct
lynx_proto_params <lynx_proto_params>`. It can also happen if you have specific
clocking requirements, or protocol-specific registers to program.

Internal API Reference
----------------------

.. kernel-doc:: drivers/phy/freescale/phy-fsl-lynx-10g.c
