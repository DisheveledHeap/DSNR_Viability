@echo off
REM ============================================
REM Windows Batch File: Alpine Linux NUMA VM
REM Default: run VM from disk
REM Optional flag: /install -> boot from ISO
REM ============================================

REM ---- Config ----
SET QEMU=qemu-system-x86_64.exe
SET DISK=numa_setup.qcow2
SET ISO=alpine-standard-x86_64.iso

SET LATENCY_VAL=32

REM Memory / SMP
SET MEM=-m 16384
SET SMP=-smp 16,sockets=1,cores=16,threads=1

REM NUMA nodes
SET NUMA=-object memory-backend-ram,size=4G,id=ram0 ^
        -object memory-backend-ram,size=4G,id=ram1 ^
        -object memory-backend-ram,size=4G,id=ram2 ^
        -object memory-backend-ram,size=4G,id=ram3 ^
        -numa node,nodeid=0,cpus=0-3,memdev=ram0 ^
        -numa node,nodeid=1,cpus=4-7,memdev=ram1 ^
        -numa node,nodeid=2,cpus=8-11,memdev=ram2 ^
        -numa node,nodeid=3,cpus=12-15,memdev=ram3

REM Network and display
SET NET=-netdev user,id=net0 -device virtio-net-pci,netdev=net0
SET VGA=-vga std

REM Artificial latency (default, can edit)
SET LATENCY=-numa dist,src=0,dst=1,val=%LATENCY_VAL% ^
		-numa dist,src=2,dst=3,val=15 ^
		-numa dist,src=0,dst=2,val=%LATENCY_VAL% ^
		-numa dist,src=0,dst=3,val=%LATENCY_VAL% ^
		-numa dist,src=1,dst=2,val=15 ^
		-numa dist,src=1,dst=3,val=15

REM ---- Check flag ----
IF /I "%1"=="/install" (
    ECHO Installing Alpine Linux from ISO...
    %QEMU% -machine q35 -cpu qemu64 %SMP% %MEM% %NUMA% -drive file=%DISK%,if=virtio -cdrom %ISO% -boot d %NET% %VGA%
) ELSE (
    ECHO Running VM from disk...
    %QEMU% -machine q35 -cpu qemu64 %SMP% %MEM% %NUMA% %LATENCY% -drive file=%DISK%,if=virtio -boot c %NET% %VGA%
)