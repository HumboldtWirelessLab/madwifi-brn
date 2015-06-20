obj := $(firstword $(obj) $(SUBDIRS) .)
TOP = $(obj)/..

obj-y := ccmp/ tkip/ wep/

include $(TOP)/Makefile.inc

modules:
	$(MAKE) -C $(KERNELPATH) SUBDIRS=$(shell pwd) modules

install:
	for i in $(obj-y); do \
		$(MAKE) -C $$i install || exit 1; \
	done

clean:
	for i in $(obj-y); do \
		$(MAKE) -C $$i clean; \
	done

	rm -f modules.order *.symvers
	rm -rf .tmp_versions
