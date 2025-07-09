Name:       villas-node
Version:    0.11
Release:    1%{?dist}
Summary:    Connecting real-time power grid simulation equipment
Group:      Applications/Engineering
License:    Apache-2.0

%description

This is VILLASnode, a gateway for processing and forwarding simulation data between real-time simulators. VILLASnode is a client/server application to connect simulation equipment and software such as:

- OPAL-RT RT-LAB,
- RTDS GTFPGA cards,
- RTDS GTWIF cards,
- Simulink,
- LabView,
- and FPGA models

by using protocols such as:

- IEEE 802.2 Ethernet / IP / UDP,
- ZeroMQ & nanomsg,
- MQTT & AMQP
- WebSockets
- Shared Memory
- Files
- IEC 61850 Sampled Values / GOOSE
- Analog/Digital IO via Comedi drivers
- Infiniband (ibverbs)

It's designed with a focus on very low latency to achieve real-time exchange of simulation data. VILLASnode is used in distributed- and co-simulation scenarios and developed for the field of power grid simulation at the EON Energy Research Center in Aachen, Germany.

%define _unpackaged_files_terminate_build 0
%define _arch x86_64

%prep

%build

PKG_CONFIG_PATH="%{buildroot}/lib/pkgconfig:%{buildroot}/lib64/pkgconfig:%{buildroot}/share/pkgconfig"
export PKG_CONFIG_PATH

PREFIX=%{buildroot}
export PREFIX

DEPS_NONINTERACTIVE=1 \
DEPS_SKIP=criterion \
bash %{srcdir}/packaging/deps.sh

rm -f %{_builddir}/CMakeCache.txt

cmake \
      -S "%{srcdir}" \
      -B "%{_builddir}" \
      -D CMAKE_INSTALL_PREFIX=%{buildroot} \
      -D CMAKE_INSTALL_RPATH=/usr/villas/lib \
      -D CMAKE_BUILD_TYPE=Release \
      -D CMAKE_IGNORE_PATH=/usr/local \
      -D WITH_FPGA=OFF \
      -D LOG_COLOR_DISABLE=ON

make -C "%{_builddir}" -j16 install

%install

# Move files under /usr/villas
mkdir -p %{buildroot}/usr/villas/{lib,bin}  %{buildroot}/usr/bin
cp -r %{buildroot}/{libexec,include}        %{buildroot}/usr/villas/
cp -r %{buildroot}/lib/{graphviz,pkgconfig} %{buildroot}/usr/villas/lib/
cp %{buildroot}/lib/*.so*                   %{buildroot}/usr/villas/lib/
cp %{buildroot}/lib64/*.so*                 %{buildroot}/usr/villas/lib/
cp %{buildroot}/bin/villas*                 %{buildroot}/usr/villas/bin/

# Create symlinks to binaries
for BIN in %{buildroot}/usr/villas/bin/*; do
    ln -st %{buildroot}/usr/bin/ /usr/villas/bin/$(basename ${BIN})
done

%files

%config(noreplace) /etc/villas/*
/usr/villas/*
/usr/bin/*

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig
