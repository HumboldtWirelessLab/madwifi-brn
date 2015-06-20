obj := $(firstword $(obj) $(SUBDIRS) .)
TOP = $(obj)/..

obj-y := amrr/ onoe/ sample/ minstrel/

include $(TOP)/Makefile.inc

.PHONY: modules
modules:
	$(MAKE) -C $(KERNELPATH) SUBDIRS=$(shell pwd) modules

.PHONY: install
install:
	for i in $(obj-y); do \
		$(MAKE) -C $$i install || exit 1; \
	done

.PHONY: clean
clean:
	for i in $(obj-y); do \
		$(MAKE) -C $$i clean; \
	done

	rm -f modules.order
