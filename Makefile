KDIR ?= /lib/modules/$(shell uname -r)/build
SRC_DIR := $(shell pwd)
BUILD_DIR := $(shell pwd)/build

SOURCES := $(wildcard $(SRC_DIR)/src/*.c)

all:
	mkdir -p $(BUILD_DIR)
	cp $(SOURCES) $(BUILD_DIR)/
	cp $(SRC_DIR)/Kbuild $(BUILD_DIR)/
	$(MAKE) -C $(KDIR) M=$(BUILD_DIR) modules

modules_install:
	$(MAKE) -C $(KDIR) M=$(BUILD_DIR) modules_install
	depmod -a

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all modules_install clean
