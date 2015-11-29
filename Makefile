#
# Copyright (c) 2012-2015, Ari Suutari <ari@stonepile.fi>.
# All rights reserved. 
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. The name of the author may not be used to endorse or promote
#     products derived from this software without specific prior written
#     permission. 
# 
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
# INDIRECT,  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
# OF THE POSSIBILITY OF SUCH DAMAGE.

RELROOT = ../picoos/

PORT = cortex-m
CPU = stm32
CORTEX = m3
export CORTEX

BUILD = DEBUG
LD_SCRIPTS=stm32.ld

WICED_CHIP      = 43362A2
WICED_MCU       = STM32F2xx
WICED_PLATFORM  = EMW3162
WICED_BUS       = SDIO

include $(RELROOT)make/common.mak

NANO = 1
TARGET = wiced-test
SRC_TXT =	 main.c  startup.c romfiles.c sock_server.c ap.c
SRC_HDR = 
SRC_OBJ =
SRC_LIB =

# CMSIS setup
CMSIS_MODULES=$(CURRENTDIR)/../cmsis-ports/stm32f2xx
STM32_DEFINES = HSE_VALUE=26000000
#STM32_CHIP=STM32F205xx
#export STM32_CHIP
MODULES += ../picoos-lwip ../wiced-driver ../eshell

DIR_CONFIG = $(CURRENTDIR)
DIR_OUTPUT = $(CURRENTDIR)/bin
MODULES +=  ../picoos-micro

POSTLINK1 = arm-none-eabi-size $(TARGETOUT)

# ---------------------------------------------------------------------------
# BUILD THE EXECUTABLE
# ---------------------------------------------------------------------------

include $(MAKE_OUT)

romfiles.c:
	sh gen_romfs.sh > romfiles.c

