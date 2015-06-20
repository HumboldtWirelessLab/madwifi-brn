#
# Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer,
#    without modification.
# 2. Redistributions in binary form must reproduce at minimum a disclaimer
#    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
#    redistribution must be conditioned upon including a substantially
#    similar Disclaimer requirement for further binary redistribution.
# 3. Neither the names of the above-listed copyright holders nor the names
#    of any contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# Alternatively, this software may be distributed under the terms of the
# GNU General Public License ("GPL") version 2 as published by the Free
# Software Foundation.
#
# NO WARRANTY
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
# AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGES.
#
# $Id: //depot/sw/linuxsrc/src/802_11/madwifi/madwifi/Makefile.inc#11 $
#

# other locales sometimes cause trouble
export LC_ALL = POSIX

# BUS defines the bus type to which the wireless devices is attached.
# Currently, the valid BUS types are PCI and AHB.  If BUS is not
# defined, then, we assume the bus type is PCI
ifndef BUS
BUS=		PCI
endif

# DESTDIR is used as path prefix during installation.
DESTDIR ?=

# By default, we try to compile the modules for the currently running
# kernel.  But it's the first approximation, as we will re-read the
# version from the kernel sources.
KVERS_UNAME ?= $(shell uname -r)

# KERNELPATH is the path to the Linux kernel build tree.  It is usually the
# same as the kernel source tree, except when the kernel was compiled in
# a separate directory.
KERNELPATH ?= $(shell readlink -f /lib/modules/$(KVERS_UNAME)/build)

ifeq (,$(KERNELPATH))
$(error Kernel build tree not found - please set KERNELPATH to configured kernel)
endif

KERNELCONF := $(KERNELPATH)/.config
ifeq (,$(wildcard $(KERNELCONF)))
$(error No .config found in $(KERNELPATH), please set KERNELPATH to configured kernel)
endif

ifneq (,$(wildcard $(KERNELPATH)/include/linux/version.h))
ifneq (,$(wildcard $(KERNELPATH)/include/generated/uapi/linux/version.h))
$(error Multiple copies of version.h found, please clean your build tree)
endif
endif

# Kernel Makefile doesn't always know the exact kernel version, so we
# get it from the kernel headers instead and pass it to make.
VERSION_H := $(KERNELPATH)/include/generated/utsrelease.h
ifeq (,$(wildcard $(VERSION_H)))
VERSION_H := $(KERNELPATH)/include/linux/utsrelease.h
endif
ifeq (,$(wildcard $(VERSION_H)))
VERSION_H := $(KERNELPATH)/include/linux/version.h
endif
ifeq (,$(wildcard $(VERSION_H)))
$(error Please run 'make modules_prepare' in $(KERNELPATH))
endif

KERNELRELEASE := $(shell sed -ne 's/"//g;s/^\#define UTS_RELEASE //p' $(VERSION_H))

ifeq (,$(KERNELRELEASE))
$(error Cannot find UTS_RELEASE in $(VERSION_H), please report)
endif

include $(KERNELCONF)

include $(TOP)/Makefile.brn

# KMODPATH nominates the directory where the modules will be
# installed to
KMODPATH :=	/lib/modules/$(KERNELRELEASE)/net

KMODSUF		:= ko
NM=		nm

#
# Path to the HAL source code.
#
ifeq ($(HAL),)
HAL=	$(TOP)/ath_hal
endif
#
# Path to the 802.11 include files.
#
WLAN=	$(TOP)/net80211
#
# Path to the Atheros device driver.
#
ATH=	$(TOP)/ath
#
# Path to the rate control algorithms.
#
ATH_RATE=	$(TOP)/ath_rate
#
# Path to the userspace utilities. 
# 
TOOLS=  $(TOP)/tools

WARNINGS = -Wall
COPTS+= $(WARNINGS)
INCS=	-include $(TOP)/include/compat.h -I$(TOP)/include

ifndef ATH_DEBUG_SPINLOCKS
export ATH_DEBUG_SPINLOCKS=0
endif

ifndef IEEE80211_DEBUG_REFCNT
export IEEE80211_DEBUG_REFCNT=0
endif

ifndef HAL_DEBUG
export HAL_DEBUG=0
endif

ifndef ATH_DEBUG
export ATH_DEBUG=1
endif

ifeq ($(strip $(ATH_DEBUG_SPINLOCKS)),1)
COPTS+= -DATH_DEBUG_SPINLOCKS=1
endif

ifeq ($(strip $(IEEE80211_DEBUG_REFCNT)),1)
COPTS+= -DIEEE80211_DEBUG_REFCNT=1
endif

# HAL_DEBUG enables HAL debugging code
# At the moment this consists of wrappers around HAL functions so that 
# stack traces are more decipherable.
ifeq ($(strip $(HAL_DEBUG)),1)
COPTS+= -fno-inline -DAH_DEBUG=1
endif

include $(TOP)/BuildCaps.inc

ifeq ($(strip $(ATH_DEBUG)),1)
EXTRA_CFLAGS += -DAR_DEBUG -DIEEE80211_DEBUG 
endif

ifeq ($(strip $(ATH_DEBUG_SPINLOCKS)),1)
EXTRA_CFLAGS += -DATH_DEBUG_SPINLOCKS
endif

ifeq ($(strip $(IEEE80211_DEBUG_REFCNT)),1)
EXTRA_CFLAGS += -DIEEE80211_DEBUG_REFCNT
endif

# Ensure correct endianess
LDFLAGS += $(LDOPTS)

# Filter out compiler options that are not supported by all compilers
# and that are not needed to ensure compatible architecture and calling
# conventions.
COPTS := $(filter-out -mshort-load-bytes -mapcs-32,$(COPTS))


# The following variables will affect developers only, and are used in the
# make-release.bash script.
#
# RELEASE_TMP is used as a temporary store for the files needed to create the
# release tarball. Packaging will take place in TMPDIR/madwifi-release.
# Defaults to /tmp.
RELEASE_TMP=/tmp

# RELEASE_STORE is the (local) directory where the release tarball should be
# stored in. Defaults to the parent directory of the working copy used to
# create the release.
RELEASE_STORE=..

# expose these settings to make-release.bash
export RELEASE_TMP RELEASE_STORE
