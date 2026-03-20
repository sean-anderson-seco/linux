.. SPDX-License-Identifier: GFDL-1.1-no-invariants-or-later
.. c:namespace:: V4L

.. _V4L2-PIX-FMT-QBGGR8:
.. _v4l2-pix-fmt-qgbrg8:
.. _v4l2-pix-fmt-qgrbg8:
.. _v4l2-pix-fmt-qrggb8:
.. _v4l2-pix-fmt-qbggr10p:
.. _v4l2-pix-fmt-qgbrg10p:
.. _v4l2-pix-fmt-qgrbg10p:
.. _v4l2-pix-fmt-qrggb10p:
.. _v4l2-pix-fmt-qbggr10dpcm8:
.. _v4l2-pix-fmt-qgbrg10dpcm8:
.. _v4l2-pix-fmt-qgrbg10dpcm8:
.. _v4l2-pix-fmt-qrggb10dpcm8:

******************
Quad Bayer Formats
******************

*man V4L2_PIX_FMT_QBGGR8(2)*

V4L2_PIX_FMT_QBGGR8
V4L2_PIX_FMT_QGBRG8
V4L2_PIX_FMT_QGRBG8
V4L2_PIX_FMT_QRGGB8
V4L2_PIX_FMT_QBGGR10P
V4L2_PIX_FMT_QGBRG10P
V4L2_PIX_FMT_QGRBG10P
V4L2_PIX_FMT_QRGGB10P
V4L2_PIX_FMT_QBGGR10DPCM8
V4L2_PIX_FMT_QGBRG10DPCM8
V4L2_PIX_FMT_QGRBG10DPCM8
V4L2_PIX_FMT_QRGGB10DPCM8
Quad (4x4) Bayer formats

Description
===========

These pixel formats are raw sRGB / Bayer formats with samples in 4x4 groups.
Each n-pixel row contains n/2 green samples and n/2 blue or red samples, with
alternating pairs of red and blue rows. They are conventionally described as
GGRRGGRR... GGRRGGRR... BBGGBBGG... BBGGBBGG... GGRRGGRR..., etc.  Aside from
the semantics of each sample, these formats are encoded identically to their
conventional Bayer (2x2) counterparts. Below is an example of a small
V4L2_PIX_FMT_QBGGR8 image:

.. flat-table::
    :header-rows:  0
    :stub-columns: 0

    * - start + 0:
      - B\ :sub:`00`
      - B\ :sub:`01`
      - G\ :sub:`02`
      - G\ :sub:`03`
      - B\ :sub:`04`
      - B\ :sub:`05`
      - G\ :sub:`06`
      - G\ :sub:`07`
    * - start + 8:
      - G\ :sub:`10`
      - G\ :sub:`11`
      - R\ :sub:`12`
      - R\ :sub:`13`
      - G\ :sub:`14`
      - G\ :sub:`15`
      - R\ :sub:`16`
      - R\ :sub:`17`
    * - start + 16:
      - B\ :sub:`20`
      - B\ :sub:`21`
      - G\ :sub:`22`
      - G\ :sub:`23`
      - B\ :sub:`24`
      - B\ :sub:`25`
      - G\ :sub:`26`
      - G\ :sub:`27`
    * - start + 24:
      - G\ :sub:`30`
      - G\ :sub:`31`
      - R\ :sub:`32`
      - R\ :sub:`33`
      - G\ :sub:`34`
      - G\ :sub:`35`
      - R\ :sub:`36`
      - R\ :sub:`37`

V4L2_PIX_FMT_QBGGR8 ('QB88'), V4L2_PIX_FMT_QGBRG8 ('QG88'), V4L2_PIX_FMT_QGRBG8 ('Qg88'), V4L2_PIX_FMT_QRGGB8 ('QR88')
======================================================================================================================

These four pixel formats are raw sRGB / Bayer formats with 8 bits per sample in
4x4 groups. See :ref:`V4L2-PIX-FMT-SRGGB8` for details.

V4L2_PIX_FMT_QBGGR10P ('qBAA'), V4L2_PIX_FMT_QGBRG10P ('qGAA'), V4L2_PIX_FMT_QGRBG10P ('qgAA'), V4L2_PIX_FMT_QRGGB10P ('qRAA')
==============================================================================================================================

These four pixel formats are packed raw sRGB / Bayer formats with 10 bits per
sample in 4x4 groups. See :ref:`V4L2-PIX-FMT-SRGGB10p` for details.

V4L2_PIX_FMT_QBGGR10DPCM8 ('QBA8'), V4L2_PIX_FMT_QGBRG10DPCM8 ('QGA8'), V4L2_PIX_FMT_QGRBG10DPCM8 ('QgA8'), V4L2_PIX_FMT_QRGGB10DPCM8 ('QRA8')
==============================================================================================================================================

These four pixel formats are raw sRGB / Bayer formats with 10 bits per samples
in 4x4 groups compressed to 8 bits each, using DPCM compression. See See
:ref:`V4L2-PIX-FMT-SRGGB10DPCM8` for details.
