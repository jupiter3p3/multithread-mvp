Goal：Demonstrate multithreaded pipeline & measurement method (userspace/QEMU)

Scenarios：baseline / affinity / queue-cap-8

Build：make

Run：scripts/run.sh → outputs metrics/metrics.csv, plots/*.png

Notes：QEMU VM、userspace C/pthreads


# Experiment Result
t000
| scenario | latency_p50_ms | latency_p95_ms | latency_p99_ms | throughput_ops_s | drop_rate |
|----------|----------------|----------------|----------------|------------------|-----------|
| baseline_pi_1000 | 0.060 | 0.122 | 0.299 | 447 | 0.000 |
| affinity_pi_1000 | 0.056 | 0.278 | 1.620 | 452 | 0.000 |
| queue_cap_8_pi_1000 | 0.062 | 0.130 | 0.345 | 456 | 0.000 |

It seemed strange that affinity had higher latency than baseline.



# Multithreaded MVP Setup Guide
2025/10/17
2025/10/18
[Windows] Install msys2:
    https://www.msys2.org/
    Get install file e.g. msys2-x86_64-20250830.exe
[MSYS2] Update msys2:
    pacman -Syu
[MSYS2] Install QEMU:
    pacman -S mingw-w64-ucrt-x86_64-qemu
[MSYS2] Install openssh:
    pacman -S openssh
[MSYS2] Create Disk image for QEMU:
    qemu-img create -f qcow2 alpine.qcow2 8G
[Windows] Download Alpine Linux ISO:
    https://alpinelinux.org/downloads/
    Get e.g. alpine-virt-3.22.2-x86_64.iso
[MSYS2] Install Alpine Linux in QEMU:
    qemu-system-x86_64.exe -m 2G -smp 2 \
    -hda alpine.qcow2 \
    -cdrom alpine-virt-3.22.2-x86_64.iso \
    -boot d \
    -nic user,hostfwd=tcp::10022-:22 \
    -k en-us
[QEMU] Install Alpine:
    Inside Alpine Linux:
        setup-alpine
        (set root password, enable networking, etc.)
[QEMU] Enable Alpine:
    qemu-system-x86_64.exe -m 2G -smp 2 \
    -hda alpine.qcow2 \
    -nic user,hostfwd=tcp::10022-:22 \
    -k en-us
[MSYS2] Connect to Alpine via SSH:
    // need to set root password
    ssh root@localhost -p 10022
[Alpine] Install git in Alpine Linux:
    apk install git
