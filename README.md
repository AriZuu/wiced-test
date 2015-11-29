This is a simple test for MXCHIP EMW3162 board with
LwIP (from picoos-lwip library) and WICED WWD driver
(from wiced-driver library).

Board has STM32F205RGT6 MCU and broadcom BCM43362A2 Wifi chip.

To build this following libraries are needed:

* Pico]OS 
* picoos-micro
* picoos-lwip
* wiced-driver
* cmsis-ports
* eshell

Get them from http://github.com/AriZuu and extract into same parent directory as this project.
Some of libraries above need source code that needs to be downloaded manually from
vendor web sites (follow instructions in the libraries).

To join a network, use "ap" command on console prompt.
