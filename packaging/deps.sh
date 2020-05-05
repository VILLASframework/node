#!/bin/bash

mkdir -p thirdparty
pushd thirdparty

# Build & Install Criterion
git clone --recursive https://github.com/Snaipe/Criterion
mkdir -p Criterion/build
pushd Criterion/build
git checkout v2.3.3
cmake -DCMAKE_INSTALL_LIBDIR=/usr/local/lib64 ..
make -j$(nproc) install
popd

# Build & Install EtherLab
hg clone --branch stable-1.5 http://hg.code.sf.net/p/etherlabmaster/code etherlab
pushd etherlab
./bootstrap
./configure --enable-userlib=yes --enable-kernel=no --enable-tool=no
make -j$(nproc) install
popd

# Build & Install Fmtlib
git clone --recursive https://github.com/fmtlib/fmt.git
mkdir -p fmt/build
pushd fmt/build
git checkout 5.2.0
cmake3 -DBUILD_SHARED_LIBS=1 ..
make -j$(nproc) install
popd

# Build & Install spdlog
git clone --recursive https://github.com/gabime/spdlog.git
mkdir -p spdlog/build
pushd spdlog/build
git checkout v1.3.1
cmake3 -DCMAKE_BUILD_TYPE=Release -DSPDLOG_FMT_EXTERNAL=1 -DSPDLOG_BUILD_BENCH=OFF ..
make -j$(nproc) install
popd

# Build & Install libiec61850
git clone -b v1.3.1 https://github.com/mz-automation/libiec61850
mkdir -p libiec61850/build
pushd libiec61850/build
cmake3 -DCMAKE_INSTALL_LIBDIR=/usr/local/lib64 ..
make -j$(nproc) install
popd

# Build & Install uldaq
git clone -b rpm https://github.com/stv0g/uldaq
pushd uldaq
autoreconf -i
./configure --enable-examples=no
make -j$(nproc) install
popd

# Build & Install EtherLab
hg clone --branch stable-1.5 http://hg.code.sf.net/p/etherlabmaster/code etherlab
pushd etherlab
./bootstrap
./configure --enable-userlib=yes --enable-kernel=no --enable-tool=no
make -j$(nproc) install
popd

# Build & Install comedilib
git clone https://github.com/Linux-Comedi/comedilib.git
pushd comedilib
git checkout r0_11_0
./autogen.sh
./configure
make -j$(nproc) install
popd

# Build & Install libre
git clone https://github.com/creytiv/re.git
pushd re
git checkout v0.6.1
make -j$(nproc) install
popd

popd