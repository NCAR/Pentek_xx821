# Pentek_xx821
C++ classes for interacting with Pentek xx821-series digital transceiver cards

This repository is intended for use as a submodule within a larger project built using the [SCons](http://scons.org)
construction tool. 

## Classes
| Class         | Description |
|---------------|-------------|
| **p_xx821**   | base class interface to a Pentek xx821-series board |
| **p_xx821Dn** | interface to one ADC/downconverter channel of a p_xx821 |
| **p_xx821Up** | interface to one DAC/upconverter channel of a p_xx821 |