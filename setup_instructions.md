# Actual Instructions
- Install [alpine linux standard iso for x86_64](https://alpinelinux.org/downloads/) 
- install and setup [qemu](https://www.qemu.org/docs/master/index.html) for your system
- install alpine linux into numa_setup qcow file with numa nodes
- exit alpine linux and use (not yet written) instructions below to relaunch qemu with access to qemu-files
- run whatever planned

note: after first installation of alpine linux into numa_setup on your device, you don't need to do it again unless changing numa setup

# Point of repository
need code that can be run in a qemu environment with numa nodes that leverages the numa nodes  
need a way of implementing numa node replication for memory management  

#### Issues
qemu uses filesystem stored in disk image, too large for github  
need to set up filesystem passthrough  
