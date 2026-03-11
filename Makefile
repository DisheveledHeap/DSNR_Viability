# Makefile: Cross-platform Alpine Linux VM with NUMA nodes and adjustable latency
# Works on Windows (WHPX), Linux (KVM), and macOS (HVF)
# CPU: qemu64 for maximum portability
# NUMA: 4 nodes x 4 cores, 16 GB RAM total

LATENCY_VAL = 254

# QEMU executable
QEMU = qemu-system-x86_64

# Disk and ISO filenames
DISK     = numa_setup.qcow2
ISO      = alpine-standard-x86_64.iso
SHAREDIR = $(CURDIR)/qemu-files

# NUMA configuration (basic)
NUMA = -object memory-backend-ram,size=4G,id=ram0 \
       -object memory-backend-ram,size=4G,id=ram1 \
       -object memory-backend-ram,size=4G,id=ram2 \
       -object memory-backend-ram,size=4G,id=ram3 \
       -numa node,nodeid=0,cpus=0-3,memdev=ram0 \
       -numa node,nodeid=1,cpus=4-7,memdev=ram1 \
       -numa node,nodeid=2,cpus=8-11,memdev=ram2 \
       -numa node,nodeid=3,cpus=12-15,memdev=ram3

# SMP and memory
SMP = -smp 16,sockets=1,cores=16,threads=1
MEM = -m 16384

# Network
NET = -netdev user,id=net0 -device virtio-net-pci,netdev=net0

# VGA / display
VGA = -vga std

# Artificial latency distances (optional, applied at runtime)
# Default: no extra latency; format: node0,node1,node2,node3
LATENCY = -numa dist,src=0,dst=1,val=$(LATENCY_VAL) \
		-numa dist,src=2,dst=3,val=15 \
		-numa dist,src=0,dst=2,val=$(LATENCY_VAL) \
		-numa dist,src=0,dst=3,val=$(LATENCY_VAL) \
		-numa dist,src=1,dst=2,val=15 \
		-numa dist,src=1,dst=3,val=15

# ---- Targets ----

# Create QCOW2 disk (if missing)
.PHONY: create-disk
create-disk:
	qemu-img create -f qcow2 $(DISK) 4G

# Install Alpine Linux (boot from ISO)
.PHONY: install
install:
	$(QEMU) -machine q35 -cpu qemu64 $(SMP) $(MEM) $(NUMA) \
	-drive file=$(DISK),if=virtio -cdrom $(ISO) -boot d $(NET) $(VGA)

# Run VM from disk (after install), optional latency
.PHONY: run
run:
	$(QEMU) -machine q35 -cpu qemu64 $(SMP) $(MEM) $(NUMA) $(LATENCY) \
	-drive file=$(DISK),if=virtio -boot c $(NET) $(VGA) \
	-fsdev local,id=fsdev0,path=$(SHAREDIR),security_model=passthrough \
	-device virtio-9p-pci,fsdev=fsdev0,mount_tag=hostshare \
	-usb -device usb-ehci,id=ehci -device usb-kbd -device usb-mouse \
	-display sdl
# run:
# 	$(QEMU) -machine q35 -cpu qemu64 $(SMP) $(MEM) $(NUMA) $(LATENCY) \
# 	-drive file=$(DISK),if=virtio -boot c $(NET) $(VGA) \
# 	-fsdev local,id=fsdev0,path=$(SHAREDIR),security_model=passthrough \
# 	-device virtio-9p-pci,fsdev=fsdev0,mount_tag=hostshare