#!/usr/bin/make -f
# SPDX-License-Identifier: GPL-2.0-only

MODULES := eiois200_core i2c-eiois200 gpio-eiois200 eiois200_bl eiois200_wdt eiois200-hwmon eiois200_fan eiois200_thermal
MODULE_VERSION := 1.0

reverse = $(if $(1),$(call reverse,$(wordlist 2,$(words $(1)),$(1)))) $(firstword $(1))
REVMODS := $(call reverse,$(MODULES))

all: $(MODULES)

eiois200_core:
	$(MAKE) -C "$@"

$(filter-out eiois200_core,$(MODULES)): eiois200_core
	$(MAKE) -C "$@"

CLEANMODS := $(addprefix clean-,$(MODULES))
$(CLEANMODS):
	$(MAKE) -C "$(subst clean-,,$@)" clean
clean: $(CLEANMODS)

LOADMODS := $(addprefix load-,$(MODULES))
$(LOADMODS):
	$(MAKE) -C "$(subst load-,,$@)" load

load: $(LOADMODS)

UNLOADMODS := $(addprefix unload-,$(REVMODS))
$(UNLOADMODS):
	$(MAKE) -C "$(subst unload-,,$@)" unload
unload: $(UNLOADMODS)

INSTALLMODS := $(addprefix install-,$(MODULES))
$(INSTALLMODS):
	$(MAKE) -C "$(subst install-,,$@)" install
install: $(INSTALLMODS)

UNINSTALLMODS := $(addprefix uninstall-,$(REVMODS))
$(UNINSTALLMODS):
	$(MAKE) -C "$(subst uninstall-,,$@)" uninstall
uninstall: $(UNINSTALLMODS)

.PHONY: all clean $(MODULES) $(CLEANMODS) $(LOADMODS) $(UNLOADMODS) $(INSTALLMODS) $(UNINSTALLMODS)
