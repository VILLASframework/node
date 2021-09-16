#!/bin/bash

set -e

PREFIX=${PREFIX:-/usr/local}
TRIPLET=${TRIPLET:-x86_64-linux-gnu}

CONFIGURE_OPTS+=" --host=${TRIPLET} --prefix=${PREFIX}"
CMAKE_OPTS+=" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${PREFIX}"

MAKE_THREADS=${MAKE_THREADS:-$(nproc)}
MAKE_OPTS+="-j${MAKE_THREADS}"

git config --global http.postBuffer 524288000
git config --global core.compression 0

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
    git clone --branch v2.3.3 --depth 1 --recursive https://github.com/Snaipe/Criterion
    mkdir -p Criterion/build
    pushd Criterion/build
    cmake ${CMAKE_OPTS} ..
    if [ -z "${PACKAGE}" ]; then
        make ${MAKE_OPTS} install
    fi
    popd
fi

# Build & Install libjansson
if ! pkg-config "jansson >= 2.7" && \
    [ -z "${SKIP_JANSSON}" ]; then
    git clone --branch v2.14 --depth 1 https://github.com/akheron/jansson
    pushd jansson
    autoreconf -i
    ./configure ${CONFIGURE_OPTS}
    if [ -z "${PACKAGE}" ]; then
        make ${MAKE_OPTS} install
    fi
    popd
fi

# Build & Install Lua
if ! ( pkg-config "lua >= 5.1" || \
       pkg-config "lua54" || \
       pkg-config "lua53" || \
       pkg-config "lua52" || \
       pkg-config "lua51" || \
       [ -n "${RTLAB_ROOT}" -a -f "/usr/local/include/lua.h" ] \
     ) && [ -z "${SKIP_LUA}" ]; then
    wget http://www.lua.org/ftp/lua-5.3.6.tar.gz -O - | tar -xz
    pushd lua-5.3.6
    if [ -z "${PACKAGE}" ]; then
        make ${MAKE_OPTS} MYCFLAGS=-fPIC linux
        make ${MAKE_OPTS} MYCFLAGS=-fPIC install
    fi
    popd
fi

# Build & Install mosquitto
if ! pkg-config "libmosquitto >= 1.4.15" && \
    [ -z "${SKIP_LIBMOSQUITTO}" ]; then
    git clone --branch v2.0.12 --depth 1 https://github.com/eclipse/mosquitto
    mkdir -p mosquitto/build
    pushd mosquitto/build
    cmake -DWITH_BROKER=OFF \
          -DWITH_CLIENTS=OFF \
          -DWITH_APPS=OFF \
          -DDOCUMENTATION=OFF \
          ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} ${TARGET}
    popd
fi

# Build & Install rabbitmq-c
if ! pkg-config "librabbitmq >= 0.8.0" && \
    [ -z "${SKIP_LIBRABBITMQ}" ]; then
    git clone --branch v0.11.0 --depth 1 https://github.com/alanxz/rabbitmq-c
    mkdir -p rabbitmq-c/build
    pushd rabbitmq-c/build
    cmake ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} ${TARGET}
    popd
fi

# Build & Install libzmq
if ! pkg-config "libzmq >= 2.2.0" && \
    [ -z "${SKIP_LIBZMQ}" ]; then
    git clone --branch v4.3.4 --depth 1 https://github.com/zeromq/libzmq
    mkdir -p libzmq/build
    pushd libzmq/build
    cmake -DWITH_PERF_TOOL=OFF \
          -DZMQ_BUILD_TESTS=OFF \
          -DENABLE_CPACK=OFF \
          ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} ${TARGET}
    popd
fi

# Build & Install EtherLab
if [ -z "${SKIP_ETHERLAB}" ]; then
    hg clone --branch stable-1.5 http://hg.code.sf.net/p/etherlabmaster/code etherlab
    pushd etherlab
    ./bootstrap
    ./configure --enable-userlib=yes --enable-kernel=no ${CONFIGURE_OPTS}
    if [ -z "${PACKAGE}" ]; then
        make ${MAKE_OPTS} install
    else
        wget https://etherlab.org/download/ethercat/ethercat-1.5.2.tar.bz2
        cp ethercat-1.5.2.tar.bz2 ~/rpmbuild/SOURCES
        rpmbuild -ba ethercat.spec
    fi
    popd
fi

# Build & Install libiec61850
if ! pkg-config "libiec61850 >= 1.3.1" && \
    [ -z "${SKIP_LIBIEC61850}" ]; then
    git clone --branch v1.4 --depth 1 https://github.com/mz-automation/libiec61850
    mkdir -p libiec61850/build
    pushd libiec61850/build
    cmake ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} ${TARGET}
    if [ -n "${PACKAGE}" ]; then
        cp libiec61850/build/*.rpm rpms
    fi
    popd
fi

# Build & Install librdkafka
if ! pkg-config "rdkafka >= 1.5.0" && \
    [ -z "${SKIP_RDKAFKA}" ]; then
    git clone --branch v1.6.0 --depth 1 https://github.com/edenhill/librdkafka
    mkdir -p librdkafka/build
    pushd librdkafka/build
    cmake -DRDKAFKA_BUILD_TESTS=OFF \
          -DRDKAFKA_BUILD_EXAMPLES=OFF \
          ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} ${TARGET}
    popd
fi

# Build & Install Graphviz
if ! ( pkg-config "libcgraph >= 2.30" && \
       pkg-config "libgvc >= 2.30" \
     ) && [ -z "${SKIP_RDKAFKA}" ]; then
    git clone --branch 2.49.0 --depth 1 https://gitlab.com/graphviz/graphviz.git
    mkdir -p graphviz/build
    pushd graphviz/build
    cmake ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} ${TARGET}
    popd
fi

# Build & Install uldaq
if ! pkg-config "libuldaq >= 1.2.0" && \
    [ -z "${SKIP_ULDAQ}" ]; then
    git clone --branch v1.2.0 --depth 1 https://github.com/mccdaq/uldaq
    pushd uldaq
    autoreconf -i
    ./configure --enable-examples=no ${CONFIGURE_OPTS}
    if [ -z "${PACKAGE}" ]; then
        make ${MAKE_OPTS} install
    else
        make dist
        cp fedora/uldaq_ldconfig.patch libuldaq-1.1.2.tar.gz ~/rpmbuild/SOURCES
        rpmbuild -ba fedora/uldaq.spec
    fi
    popd
fi

# Build & Install libnl3
if ! ( pkg-config "libnl-3.0 >= 3.2.25" && \
       pkg-config "libnl-route-3.0 >= 3.2.25" \
     ) && [ -z "${SKIP_ULDAQ}" ]; then
    git clone --branch libnl3_5_0 --depth 1 https://github.com/thom311/libnl
    pushd libnl
    autoreconf -i
    ./configure ${CONFIGURE_OPTS}
    if [ -z "${PACKAGE}" ]; then
        make ${MAKE_OPTS} install
    fi
    popd
fi

# Build & Install libconfig
if ! pkg-config "libconfig >= 1.4.9" && \
    [ -z "${SKIP_ULDAQ}" ]; then
    git clone --branch v1.7.3 --depth 1 https://github.com/hyperrealm/libconfig
    pushd libconfig
    autoreconf -i
    ./configure ${CONFIGURE_OPTS} \
        --disable-tests \
        --disable-examples \
        --disable-doc
    if [ -z "${PACKAGE}" ]; then
        make ${MAKE_OPTS} install
    fi
    popd
fi

# Build & Install comedilib
if ! pkg-config "comedilib >= 0.11.0" && \
    [ -z "${SKIP_COMEDILIB}" ]; then
    git clone --branch r0_12_0 --depth 1 https://github.com/Linux-Comedi/comedilib.git
    pushd comedilib
    ./autogen.sh
    ./configure ${CONFIGURE_OPTS}
    if [ -z "${PACKAGE}" ]; then
        make ${MAKE_OPTS} install
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
    git clone --branch v0.6.1 --depth 1 https://github.com/creytiv/re.git
    pushd re
    if [ -z "${PACKAGE}" ]; then
        make ${MAKE_OPTS} install
    else
        tar --transform 's|^\.|re-0.6.1|' -czvf ~/rpmbuild/SOURCES/re-0.6.1.tar.gz .
        rpmbuild -ba rpm/re.spec
    fi
    popd
fi

# Build & Install nanomsg
if ! pkg-config "nanomsg >= 1.0.0" && \
    [ -z "${SKIP_NANOMSG}" ]; then
    git clone --branch 1.1.5 --depth 1 https://github.com/nanomsg/nanomsg.git
    mkdir -p nanomsg/build
    pushd nanomsg/build
    cmake ${CMAKE_OPTS} ..
    if [ -z "${PACKAGE}" ]; then
        make ${MAKE_OPTS} install
    fi
    popd
fi

# Build & Install libxil
if ! pkg-config "libxil >= 1.0.0" && \
    [ -z "${SKIP_LIBXIL}" ]; then
    git clone --branch v0.1.0 --depth 1 https://git.rwth-aachen.de/acs/public/villas/fpga/libxil.git
    mkdir -p libxil/build
    pushd libxil/build
    cmake ${CMAKE_OPTS} ..
    if [ -z "${PACKAGE}" ]; then
        make ${MAKE_OPTS} install
    fi
    popd
fi

# Build & Install hiredis
if ! pkg-config "hiredis>1.0.0" && \
    [ -z "${SKIP_HIREDIS}" -a -z "${SKIP_REDIS}" ]; then
    git clone --branch v1.0.0 --depth 1 https://github.com/redis/hiredis.git
    mkdir -p hiredis/build
    pushd hiredis/build
    cmake -DDISABLE_TESTS=ON \
          -DENABLE_SSL=ON \
          ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} ${TARGET}
    popd
fi

# Build & Install redis++
if [ -z "${SKIP_REDISPP}" -a -z "${SKIP_REDIS}" ]; then
    git clone --branch 1.2.3 --depth 1 https://github.com/sewenew/redis-plus-plus.git
    mkdir -p redis-plus-plus/build
    pushd redis-plus-plus/build

    # Somehow redis++ fails to find the hiredis include path on Debian multiarch builds
    REDISPP_CMAKE_OPTS+="-DCMAKE_CXX_FLAGS=-I/usr/local/include"

    cmake -DREDIS_PLUS_PLUS_BUILD_STATIC=OFF \
          -DREDIS_PLUS_PLUS_CXX_STANDARD=17 \
          ${REDISPP_CMAKE_OPTS} ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} ${TARGET} VERBOSE=1
    popd
fi

# Build & Install Fmtlib
if ! pkg-config "fmt >= 6.1.2" && \
    [ -z "${SKIP_FMTLIB}" ]; then
    git clone --branch 6.1.2 --depth 1 --recursive https://github.com/fmtlib/fmt.git
    mkdir -p fmt/build
    pushd fmt/build
    cmake -DBUILD_SHARED_LIBS=1 \
        ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} ${TARGET}
    if [ -n "${PACKAGE}" ]; then
        cp fmt/build/*.rpm rpms
    fi
    popd
fi

# Build & Install spdlog
if ! pkg-config "spdlog >= 1.8.2" && \
    [ -z "${SKIP_SPDLOG}" ]; then
    git clone --branch v1.8.2 --depth 1 --recursive https://github.com/gabime/spdlog.git
    mkdir -p spdlog/build
    pushd spdlog/build
    cmake -DSPDLOG_FMT_EXTERNAL=ON \
          -DSPDLOG_BUILD_BENCH=OFF \
          -DSPDLOG_BUILD_SHARED=ON \
          ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} ${TARGET}
    if [ -n "${PACKAGE}" ]; then
        cp spdlog/build/*.rpm rpms
    fi
    popd
fi

# Build & Install libwebsockets
if ! pkg-config "libwebsockets >= 2.3.0" && \
    [ -z "${SKIP_WEBSOCKETS}" ]; then
    git clone --branch v4.0-stable --depth 1 https://libwebsockets.org/repo/libwebsockets
    mkdir -p libwebsockets/build
    pushd libwebsockets/build
    cmake -DLWS_WITH_IPV6=ON \
          -DLWS_WITHOUT_TESTAPPS=ON \
          -DLWS_WITHOUT_EXTENSIONS=OFF \
          -DLWS_WITH_SERVER_STATUS=ON \
          ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} ${TARGET}
    popd
fi

if [ -n "${PACKAGE}" ]; then
    cp ~/rpmbuild/RPMS/x86_64/*.rpm rpms
fi

popd
rm -rf ${DIR}

# Update linker cache
if [ -z "${PACKAGE}" -a -z "${SKIP_LDCONFIG}" ]; then
    ldconfig
fi
