OS := $(shell uname -s)
ARCH := $(shell uname -m)

ifeq ($(ARCH),x86_64)
    UBUNTU_ARCH := amd64
    QEMU_BIN := qemu-system-x86_64
    KERNEL_ARCH := x86
    QEMU_MACHINE_ARGS :=
else ifneq (,$(filter $(ARCH),aarch64 arm64))
    UBUNTU_ARCH := arm64
    QEMU_BIN := qemu-system-aarch64
    KERNEL_ARCH := arm64
    QEMU_MACHINE_ARGS := -machine virt -cpu host -bios /usr/share/qemu/edk2-aarch64-code.fd
else
    $(error Unsupported architecture: $(ARCH))
endif

ifeq ($(OS),Darwin)
    ACCEL := hvf
else
    ACCEL := kvm
endif

VM_DIR      := $(CURDIR)/.vm
REPO_ROOT   := $(abspath $(CURDIR)/..)
KHEADERS    := $(REPO_ROOT)/.kheaders

IMG_URL     := https://cloud-images.ubuntu.com/noble/current/noble-server-cloudimg-$(UBUNTU_ARCH).img
CLOUD_IMG   := $(VM_DIR)/noble-$(UBUNTU_ARCH).img
OVERLAY     := $(VM_DIR)/overlay.qcow2
SEED_ISO    := $(VM_DIR)/seed.iso
USER_DATA   := $(VM_DIR)/user-data
META_DATA   := $(VM_DIR)/meta-data
PIDFILE     := $(VM_DIR)/qemu.pid

SSH_KEY     := $(HOME)/.ssh/id_ed25519.pub
SSH_IDENTITY := $(HOME)/.ssh/id_ed25519
SSH_PORT    := 2222
SSH_OPTS    := -i $(SSH_IDENTITY) -o IdentitiesOnly=yes \
               -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
               -o LogLevel=ERROR -p $(SSH_PORT)
SSH         := ssh $(SSH_OPTS) root@localhost

setup: $(CLOUD_IMG) $(OVERLAY) $(SEED_ISO)

$(VM_DIR):
	mkdir -p $(VM_DIR)

$(CLOUD_IMG): | $(VM_DIR)
	curl -fL -o $@ $(IMG_URL)

$(OVERLAY): $(CLOUD_IMG)
	qemu-img create -f qcow2 -F qcow2 -b $(CLOUD_IMG) $@ 20G

$(SEED_ISO): cloud-config-template.yaml meta-data.yaml $(SSH_KEY) | $(VM_DIR)
	awk -v key="$$(cat $(SSH_KEY))" '{gsub(/\{\{SSH_PUBLIC_KEY\}\}/, key); print}' \
	    cloud-config-template.yaml > $(USER_DATA)
	cp meta-data.yaml $(META_DATA)
	xorrisofs -output $@ -volid cidata -joliet -rock $(USER_DATA) $(META_DATA)

start: $(SEED_ISO) $(OVERLAY)
	$(QEMU_BIN) \
	    -accel $(ACCEL) \
	    $(QEMU_MACHINE_ARGS) \
	    -m 2048 -smp 2 \
	    -drive file=$(OVERLAY),if=virtio,format=qcow2 \
	    -drive file=$(SEED_ISO),if=virtio,format=raw \
	    -netdev user,id=net0,hostfwd=tcp::$(SSH_PORT)-:22 \
	    -device virtio-net-pci,netdev=net0 \
	    -virtfs local,path=$(REPO_ROOT),mount_tag=host0,security_model=none,id=host0 \
	    -display none \
	    -serial file:$(VM_DIR)/console.log \
	    -daemonize \
	    -pidfile $(PIDFILE)
	@echo "Waiting for SSH on port $(SSH_PORT)..."
	@for i in $$(seq 1 60); do \
	    $(SSH) -o ConnectTimeout=2 true 2>/dev/null && echo "VM ready." && exit 0; \
	    sleep 5; \
	done; \
	echo "Timed out waiting for SSH." >&2; exit 1

stop:
	$(SSH) 'poweroff' || true
	@rm -f $(PIDFILE)

provision:
	$(SSH) 'set -e; \
	    apt-get update; \
	    DEBIAN_FRONTEND=noninteractive apt-get install -y \
	        build-essential linux-headers-$$(uname -r) rsync; \
	    modprobe 9p; \
	    modprobe 9pnet; \
	    modprobe 9pnet_virtio; \
	    mkdir -p /mnt/host; \
	    grep -q "^host0 " /etc/fstab || \
	        echo "host0 /mnt/host 9p trans=virtio,version=9p2000.L,rw,_netdev 0 0" >> /etc/fstab; \
	    mount -a'

sync-headers:
	mkdir -p $(KHEADERS)
	rsync -az --delete -e 'ssh $(SSH_OPTS)' \
	    'root@localhost:/lib/modules/'"$$($(SSH) 'uname -r')"'/build/include/' \
	    $(KHEADERS)/include/
	rsync -az --delete -e 'ssh $(SSH_OPTS)' \
	    'root@localhost:/lib/modules/'"$$($(SSH) 'uname -r')"'/build/arch/' \
	    $(KHEADERS)/arch/

.PHONY: setup start stop provision sync-headers
