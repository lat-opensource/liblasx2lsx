# SPDX-License-Identifier: GPL-2.0-only
KVER ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVER)/build
PWD  := $(shell pwd)

.PHONY: all modules rt-blob clean

all: modules

rt-blob:
	@if [ -d "$(PWD)/rt" ]; then \
		$(MAKE) -C "$(PWD)/rt"; \
	elif [ ! -f "$(PWD)/lasx_rt.bin" ] || [ ! -f "$(PWD)/lasx_rt_meta.h" ]; then \
		echo "Error: lasx_rt.bin / lasx_rt_meta.h missing in $(PWD)"; exit 1; \
	fi

lasx_rt.bin lasx_rt_meta.h:
	$(MAKE) rt-blob

modules: lasx_rt.bin lasx_rt_meta.h
	unset ARCH; $(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	@if [ -d "$(KDIR)" ]; then \
		unset ARCH; $(MAKE) -C $(KDIR) M=$(PWD) clean; \
	else \
		rm -f *.o *.ko *.mod *.mod.c *.symvers *.order .*.cmd; \
	fi
	@if [ -d "$(PWD)/rt" ]; then \
		$(MAKE) -C "$(PWD)/rt" clean; \
	fi
