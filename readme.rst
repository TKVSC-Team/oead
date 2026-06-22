======
 tkvsc-oead
======

**tkvsc-oead** is a C++ library for common file formats that are used in modern first-party Nintendo EAD (now EPD) titles. Forked from the original **oead** library by leoetlino, it is now maintained by the TKVSC Team.
Changes made in [DT's PR](https://github.com/zeldamods/oead/pull/36) were also included

WARNING
========
This library should not be used in the same environment as the original oead library, as they have the same Python module name and will conflict with each other. If you want to use both libraries, it is recommended to use virtual environments to keep them separate.

Features
========

Currently, tkvsc-oead only handles very common formats that are extensively used in recent games such as *Breath of the Wild* and *Super Mario Odyssey*.

* `AAMP <https://zeldamods.org/wiki/AAMP>`_ (binary parameter archive): Only version 2 is supported.
* `BYML <https://zeldamods.org/wiki/BYML>`_ (binary YAL): Versions 1-10 are supported (32-bit and 64-bit hash nodes, monotyped arrays). Remapped container nodes and non-container root nodes are not supported.
* `SARC <https://zeldamods.org/wiki/SARC>`_ (archive)
* `Yaz0 <https://zeldamods.org/wiki/Yaz0>`_ (compression algorithm)

tkvsc-oead also supports a recent Grezzo format that is used in *Link's Awakening (Switch)*:

* `gsheet <https://zeldamods.org/las/Datasheet>`_ (Grezzo datasheet)

Getting started
===============

To install the Python module, simply run:

   pip install tkvsc-oead

This will download and install a precompiled version of tkvsc-oead for the following platforms:

* Windows (x86-64 / 64-bit)
* Recent Linux distributions (x86-64, glibc and musl)
* macOS 10.14+ (x86-64 / arm64)

The following versions of Python are supported:

* Python 3.13+

If you are using any other platform, you must build tkvsc-oead from source (refer to the next section).

.. warning::
   Windows users must ensure that they have the `latest Visual C++ 2019 Redistributable <https://support.microsoft.com/en-us/help/2977003/the-latest-supported-visual-c-downloads>`_ installed.

For more information, refer to the `documentation <https://oead.readthedocs.io/>`_.

Building from source
====================

Building oead from source requires:

* CMake 3.12+
* A compiler that supports C++17
* Everything needed to build libyaml

First, clone the repository then enter the oead directory and run ``git submodule update --init --recursive``.

Building the Python module
--------------------------

* To install the module, run ``pip install -e .``. This requires the following Python modules to be installed: setuptools, wheel
* If you just want to build the Python module from source without installing it, run ``python setup.py bdist_wheel``.

C++ usage
---------

Linking to the ``oead`` target is sufficient to use the library.


Contributing
============

* Issue tracker: `<https://github.com/zeldamods/oead/issues>`_
* Source code: `<https://github.com/zeldamods/oead>`_

This project is licensed under the GPLv2+ license.
