# Configuration {#Configuration}

## File

The server gets exclusively configured by its configuration file.

The following sections describes the available options.
Take a look in the 'Examples' section for some examples.

### Global

- *name*:
- *affinity*

### Nodes

### Paths

## Tuning

### Operating System and Kernel

For minimum latency several kernel and driver settings can be optimized.
A [RT-preemp patched Linux](https://rt.wiki.kernel.org/index.php/Main_Page) kernel is recommended.
Precompiled kernels for Fedora can be found here: http://ccrma.stanford.edu/planetccrma/software/

- Map NIC IRQs	=> see setting `affinity`
- Map Tasks	=> see setting `affinity`
  - Kernel command line: isolcpus=[cpu_number]
- Nice Task	=> see setting `priority`
- Increase socket priority
- Configure NIC interrupt coalescence with `ethtool`:

	$ ethtool -C|--coalesce devname [adaptive-rx on|off] [adaptive-tx on|off] ...

- Configure NIC kernel driver in `/etc/modprobe.d/e1000e.conf`:

	# More conservative interrupt throttling for better latency
	# https://www.kernel.org/doc/Documentation/networking/e1000e.txt
	option e1000e InterruptThrottleRate=

### Hardware

This are some proposals for the selection of appropriate server hardware:

- Server-grade CPU: Intel Xeon
  - A multi-core systems allows parallization of send/receive paths.
- Server-grade network cards: Intel PRO/1000
  - These allow offloading of UDP checksumming to the hardware

\example server/etc/loopback.conf
\example server/etc/example.conf
