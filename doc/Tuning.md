# Tuning {#tuning}

## Operating System and Kernel

For minimum latency several kernel and driver settings can be optimized.
A [PREEMPT_RT patched Linux](https://rt.wiki.kernel.org/index.php/Main_Page) kernel is recommended.
Precompiled kernels for Fedora can be found here: http://ccrma.stanford.edu/planetccrma/software/

- Install `tuned` package and activate the `realtime` profile. This profile will:
  - Reserve some CPU cores solely for S2SS (Kernel cmdline: `isolcpus=[cpu_numbers]`)
  - Activate sub-profiles:
     - `network-latency`
     - `latency-performance`
  - See `/etc/tuned/realtime-variables.conf`
  - See `/usr/lib/tuned/realtime/`

- S2SS configuration:
  - `affinity`: Maps network card IRQs and threads to isolated cores
  - `priority`: Increases priority of network packets and threads

- Configure NIC interrupt coalescence with `ethtool`:

    $ ethtool -C|--coalesce devname [adaptive-rx on|off] [adaptive-tx on|off] ...

- Configure NIC kernel driver in `/etc/modprobe.d/e1000e.conf`:

    # More conservative interrupt throttling for better latency
    # https://www.kernel.org/doc/Documentation/networking/e1000e.txt
    option e1000e InterruptThrottleRate=

## Hardware

This are some proposals for the selection of appropriate server hardware:

- Server-grade CPU: Intel Xeon
  - A multi-core systems allows parallization of send/receive paths.

- Server-grade network cards: Intel PRO/1000
  - These allow offloading of UDP checksumming to the hardware
