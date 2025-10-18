Goal: Demonstrate a multithreaded pipeline and a measurement method (userspace/QEMU)

Scenarios: baseline / affinity / queue-cap-8

Build：make

Run: scripts/run.sh → outputs metrics/*.csv, plots/*.png

Notes: QEMU VM, userspace C/pthreads

See **[RESULTS.md](RESULTS.md)** for the summary table and plots.

# Multithreaded MVP Setup Guide
## [Windows] Install msys2

- Download from: https://www.msys2.org/
- Get installer, e.g. `msys2-x86_64-20250830.exe`

## [MSYS2] Update msys2

```sh
pacman -Syu
```

## [MSYS2] Install QEMU
```sh
pacman -S mingw-w64-ucrt-x86_64-qemu
```

## [MSYS2] Install openssh
```sh
pacman -S openssh
``` 
## [MSYS2] Create Disk image for QEMU:
```sh
    qemu-img create -f qcow2 alpine.qcow2 8G
``` 
## [Windows] Download Alpine Linux ISO:
    - https://alpinelinux.org/downloads/
    - Get e.g. alpine-virt-3.22.2-x86_64.iso
## [MSYS2] Install Alpine Linux in QEMU:
```sh
    qemu-system-x86_64.exe -m 2G -smp 2 \
    -hda alpine.qcow2 \
    -cdrom alpine-virt-3.22.2-x86_64.iso \
    -boot d \
    -nic user,hostfwd=tcp::10022-:22 \
    -k en-us
```
## [QEMU] Install Alpine:
    setup-alpine
    # (set root password, enable networking, etc.)

## [MSYS2] Enable Alpine:
```sh
    qemu-system-x86_64.exe -m 2G -smp 2 \
    -hda alpine.qcow2 \
    -nic user,hostfwd=tcp::10022-:22 \
    -k en-us
```
## [MSYS2] Connect to Alpine via SSH:
Need to set root password first
```sh
ssh root@localhost -p 10022
```

## [Alpine] Install git in Alpine Linux:
```sh
    apk install git
```
