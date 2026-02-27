#!/bin/bash

# ------------------------------
# NUMA QEMU Launch Script
# 4 NUMA nodes
# 4 cores per node
# 4GB RAM per node
# Total: 16 vCPUs, 16GB RAM
# ------------------------------

DISK_IMAGE="numa_setup.qcow2"

if [ ! -f "$DISK_IMAGE" ]; then
    echo "Disk image $DISK_IMAGE not found!"
    exit 1
fi

qemu-system-x86_64 \
  -machine q35 \
  -enable-kvm \
  -cpu host \
  -smp 16,sockets=1,cores=16,threads=1 \
  -m 16384 \
  \
  -object memory-backend-ram,size=4G,id=ram0 \
  -object memory-backend-ram,size=4G,id=ram1 \
  -object memory-backend-ram,size=4G,id=ram2 \
  -object memory-backend-ram,size=4G,id=ram3 \
  \
  -numa node,nodeid=0,cpus=0-3,memdev=ram0 \
  -numa node,nodeid=1,cpus=4-7,memdev=ram1 \
  -numa node,nodeid=2,cpus=8-11,memdev=ram2 \
  -numa node,nodeid=3,cpus=12-15,memdev=ram3 \
  \
  -numa dist,src=0,dst=1,val=20 \
  -numa dist,src=1,dst=0,val=20 \
  -numa dist,src=0,dst=2,val=30 \
  -numa dist,src=2,dst=0,val=30 \
  -numa dist,src=0,dst=3,val=40 \
  -numa dist,src=3,dst=0,val=40 \
  \
  -drive file=$DISK_IMAGE,if=virtio \
  -net nic -net user \
  -nographic