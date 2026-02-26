KDIR ?= /lib/modules/$(shell uname -r)/build
SRC_DIR := $(shell pwd)
BUILD_DIR := $(shell pwd)/build

all:
	mkdir -p $(BUILD_DIR)
	$(MAKE) -C $(KDIR) M=$(SRC_DIR) MO=$(BUILD_DIR) modules

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
