# Tuning {#tuning}

## Operating System and Kernel

For minimum latency several kernel and driver settings can be optimized.
A [PREEMPT_RT patched Linux](https://rt.wiki.kernel.org/index.php/Main_Page) kernel is recommended.
Precompiled kernels for Fedora can be found here: http://ccrma.stanford.edu/planetccrma/software/

1. **Important:** Tune overall system performance for real-time:

```bash
$ dnf install tuned-profiles-realtime
$ echo "realtime" > /etc/tuned/active_profile
$ echo "isolated_cpucores=6-7" >> /etc/tuned/realtime-variables.conf
$ systemctl enable tuned && systemctl start tuned
```
    
  This enables the following `tuned` profiles:
    - latency-performance
      - network-latency
        - realtime

2. Map NIC IRQs	=> see setting `affinity`
3. Map Tasks	=> see setting `affinity`

4. Increase priority of server task (nice(2)) => see setting `priority`
5. Increase BSD socket priority => see setting `priority` and node-type `socket`

6. Configure NIC interrupt coalescence with `ethtool`:

```bash
$ ethtool -C|--coalesce devname [adaptive-rx on|off] [adaptive-tx on|off] ...
```

7. Configure NIC kernel driver in `/etc/modprobe.d/e1000e.conf`:

```
# More conservative interrupt throttling for better latency
# https://www.kernel.org/doc/Documentation/networking/e1000e.txt
option e1000e InterruptThrottleRate=
```

## Hardware

This are some proposals for the selection of appropriate server hardware:

- Server-grade CPU: Intel Xeon
  - A multi-core systems allows parallization of send/receive paths.

- Server-grade network cards: Intel PRO/1000
  - These allow offloading of UDP checksumming to the hardware
