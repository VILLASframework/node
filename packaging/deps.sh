#!/bin/bash

set -e

PREFIX=${PREFIX:-/usr/local}
TRIPLET=${TRIPLET:-x86_64-linux-gnu}

CONFIGURE_OPTS+=" --host=${TRIPLET} --prefix=${PREFIX}"
CMAKE_OPTS+=" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${PREFIX}"

if [ -n "${PACKAGE}" ]; then
    TARGET="package"
    CMAKE_OPTS+=" -DCPACK_GENERATOR=RPM"

    # Prepare rpmbuild dir
    mkdir -p ~/rpmbuild/SOURCES
    mkdir -p rpms

    dnf -y install \
        xmlto \
        systemd-devel
else
    TARGET="install"
fi

DIR=$(mktemp -d)
pushd ${DIR}

# Build & Install Criterion
if ! pkg-config "criterion >= 2.3.1" && \
   [ "${ARCH}" == "x86_64" ] && \
   [ -z "${SKIP_CRITERION}" ]; then
    git clone --recursive https://github.com/Snaipe/Criterion
    mkdir -p Criterion/build
    pushd Criterion/build
    git checkout v2.3.3
    cmake ${CMAKE_OPTS} ..
    if [ -z "${PACKAGE}" ]; then
        make -j$(nproc) install
    fi
    popd
fi

# Build & Install EtherLab
if [ -z "${SKIP_CRITERION}" ]; then
    hg clone --branch stable-1.5 http://hg.code.sf.net/p/etherlabmaster/code etherlab
    pushd etherlab
    ./bootstrap
    ./configure --enable-userlib=yes --enable-kernel=no ${CONFIGURE_OPTS}
    if [ -z "${PACKAGE}" ]; then
        make -j$(nproc) install
    else
        wget https://etherlab.org/download/ethercat/ethercat-1.5.2.tar.bz2
        cp ethercat-1.5.2.tar.bz2 ~/rpmbuild/SOURCES
        rpmbuild -ba ethercat.spec
    fi
    popd
fi

# Build & Install Fmtlib
if ! pkg-config "fmt >= 6.1.2"; then
    git clone --recursive https://github.com/fmtlib/fmt.git
    mkdir -p fmt/build
    pushd fmt/build
    git checkout 6.1.2
    cmake -DBUILD_SHARED_LIBS=1 ${CMAKE_OPTS} ..
    make -j$(nproc) ${TARGET}
    if [ -n "${PACKAGE}" ]; then
        cp fmt/build/*.rpm rpms
    fi
    popd
fi

# Build & Install spdlog
if ! pkg-config "spdlog >= 1.5.0"; then
    git clone --recursive https://github.com/gabime/spdlog.git
    mkdir -p spdlog/build
    pushd spdlog/build
    git checkout v1.5.0
    cmake -DSPDLOG_FMT_EXTERNAL=ON -DSPDLOG_BUILD_BENCH=OFF -DSPDLOG_BUILD_SHARED=ON ${CMAKE_OPTS} ..
    make -j$(nproc) ${TARGET}
    if [ -n "${PACKAGE}" ]; then
        cp spdlog/build/*.rpm rpms
    fi
    popd
fi

# Build & Install libiec61850
if ! pkg-config "libiec61850 >= 1.3.1"; then
    git clone https://github.com/mz-automation/libiec61850
    mkdir -p libiec61850/build
    pushd libiec61850/build
    git checkout v1.4
    cmake ${CMAKE_OPTS} ..
    make -j$(nproc) ${TARGET}
    if [ -n "${PACKAGE}" ]; then
        cp libiec61850/build/*.rpm rpms
    fi
    popd
fi

# Build & Install libwebsockets
if ! pkg-config "libwebsockets >= 2.3.0"; then
    git clone https://libwebsockets.org/repo/libwebsockets
    mkdir -p libwebsockets/build
    pushd libwebsockets/build
    git checkout v4.0-stable
    cmake -DLWS_WITHOUT_TESTAPPS=ON -DLWS_WITHOUT_EXTENSIONS=OFF -DLWS_WITH_SERVER_STATUS=ON ${CMAKE_OPTS} ..
    make -j$(nproc) ${TARGET}
    popd
fi

# Build & Install uldaq
if ! pkg-config "libuldaq >= 1.0.0" && \
   [ "${DISTRO}" != "debian-multiarch" ]; then
    git clone https://github.com/stv0g/uldaq
    pushd uldaq
    git checkout rpmbuild
    autoreconf -i
    ./configure --enable-examples=no ${CONFIGURE_OPTS}
    if [ -z "${PACKAGE}" ]; then
        make -j$(nproc) install
    else
        make dist
        cp fedora/uldaq_ldconfig.patch libuldaq-1.1.2.tar.gz ~/rpmbuild/SOURCES
        rpmbuild -ba fedora/uldaq.spec
    fi
    popd
fi

# Build & Install comedilib
if ! pkg-config "comedilib >= 0.11.0"; then
    git clone https://github.com/Linux-Comedi/comedilib.git
    pushd comedilib
    git checkout r0_12_0
    ./autogen.sh
    ./configure ${CONFIGURE_OPTS}
    if [ -z "${PACKAGE}" ]; then
        make -j$(nproc) install
    else
        touch doc/pdf/comedilib.pdf # skip build of PDF which is broken..
        make dist
        cp comedilib-0.12.0.tar.gz ~/rpmbuild/SOURCES
        rpmbuild -ba comedilib.spec
    fi
    popd
fi

# Build & Install libre
if ! pkg-config "libre >= 0.5.6" && \
    [ -z "${SKIP_LIBRE}" ]; then
    git clone https://github.com/creytiv/re.git
    pushd re
    git checkout v0.6.1
    if [ -z "${PACKAGE}" ]; then
        make -j$(nproc) install
    else
        tar --transform 's|^\.|re-0.6.1|' -czvf ~/rpmbuild/SOURCES/re-0.6.1.tar.gz .
        rpmbuild -ba rpm/re.spec
    fi
    popd
fi

# Build & Install nanomsg
if ! pkg-config "nanomsg >= 1.0.0"; then
    git clone https://github.com/nanomsg/nanomsg.git
    mkdir -p nanomsg/build
    pushd nanomsg/build
    cmake ${CMAKE_OPTS} ..
    if [ -z "${PACKAGE}" ]; then
        make -j$(nproc) install
    fi
    popd
fi

# Build & Install libxil
if ! pkg-config "libxil >= 1.0.0"; then
    git clone https://git.rwth-aachen.de/acs/public/villas/fpga/libxil.git
    mkdir -p libxil/build
    pushd libxil/build
    git checkout b622ddef4315b7e8a56637d07aa2b26ba480d53a
    cmake ${CMAKE_OPTS} ..
    if [ -z "${PACKAGE}" ]; then
        make -j$(nproc) install
    fi
    popd
fi

if [ -n "${PACKAGE}" ]; then
    cp ~/rpmbuild/RPMS/x86_64/*.rpm rpms
fi

popd
rm -rf ${DIR}

# Update linker cache
if [ -z "${PACKAGE}" ]; then
    ldconfig
fi
