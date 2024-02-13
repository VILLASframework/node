#!/bin/bash
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

# Abort the script on any failed command
set -e

# Abort the script using on undefined variables
set -u

# Aborts the script on any failing program in a pipe
set -o pipefail

should_build() {
    local id="$1"
    local use="$2"
    local requirement="${3:-optional}"

    case "${requirement}" in
        optional) ;;
        required) ;;
        *)
            echo >&2 "Error: invalid parameter '$2' for should_build. should be one of 'optional' and 'required', default is 'optional'"
            exit 1
            ;;
    esac

    local deps="${@:4}"

    if [[ -n "${DEPS_SCAN+x}" ]]; then
        echo "${requirement} dependendency ${id} should be installed ${use}."
        [[ -n "${deps[*]}" ]] && echo " transitive dependencies: ${deps}"
        echo
        return 1
    fi

    if { [[ "${DEPS_SKIP:-}" == *"${id}"* ]] || { [[ -n "${DEPS_INCLUDE+x}" ]] && [[ ! "${DEPS_INCLUDE}" == *"${id}"* ]]; }; }
    then
        echo "Skipping ${requirement} dependency '${id}'"
        return 1
    fi

    if [[ -z "${DEPS_NONINTERACTIVE+x}" ]] && [[ -t 1 ]]; then
        echo
        read -p "Do you wan't to install '${id}' into '${PREFIX}'? This is used ${use}. (y/N) "
        case "${REPLY}" in
            y | Y)
                echo "Installing '${id}'"
                return 0
                ;;

            *)
                echo "Skipping '${id}'"
                return 1
                ;;
        esac
    fi

    return 0
}

## Build configuration

# Use shallow git clones to speed up downloads
GIT_OPTS+=" --depth=1 --config advice.detachedHead=false"

# Install destination
PREFIX=${PREFIX:-/usr/local}

# Cross-compile
TRIPLET=${TRIPLET:-$(gcc -dumpmachine)}
ARCH=${ARCH:-$(uname -m)}

# CMake
CMAKE_OPTS+=" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${PREFIX}"

# Autotools
CONFIGURE_OPTS+=" --host=${TRIPLET} --prefix=${PREFIX}"

# Make
MAKE_THREADS=${MAKE_THREADS:-$(nproc)}
MAKE_OPTS+="--jobs=${MAKE_THREADS}"

# pkg-config
PKG_CONFIG_PATH=${PKG_CONFIG_PATH:-}${PKG_CONFIG_PATH:+:}${PREFIX}/lib/pkgconfig:${PREFIX}/lib64/pkgconfig:${PREFIX}/share/pkgconfig
export PKG_CONFIG_PATH

# Build in a temporary directory
TMPDIR=$(mktemp -d)

echo "Entering ${TMPDIR}"
pushd ${TMPDIR} >/dev/null

# Build & Install Criterion
if ! pkg-config "criterion >= 2.4.1" && \
   [ "${ARCH}" == "x86_64" ] && \
   should_build "criterion" "for unit tests"; then
    git clone ${GIT_OPTS} --branch v2.3.3 --recursive https://github.com/Snaipe/Criterion.git
    mkdir -p Criterion/build
    pushd Criterion/build
    cmake ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} install
    popd
fi

# Build & Install libjansson
if ! pkg-config "jansson >= 2.13" && \
    should_build "jansson" "for configuration parsing" "required"; then
    git clone ${GIT_OPTS} --branch v2.14 https://github.com/akheron/jansson.git
    pushd jansson
    autoreconf -i
    ./configure ${CONFIGURE_OPTS}
    make ${MAKE_OPTS} install
    popd
fi

# Build & Install Lua
if ! ( pkg-config "lua >= 5.1" || \
       pkg-config "lua54" || \
       pkg-config "lua53" || \
       pkg-config "lua52" || \
       pkg-config "lua51" || \
       { [[ -n "${RTLAB_ROOT:+x}" ]] && [[ -f "/usr/local/include/lua.h" ]]; } \
     ) && should_build "lua" "for the lua hook"; then
    wget http://www.lua.org/ftp/lua-5.4.4.tar.gz -O - | tar -xz
    pushd lua-5.4.4
    make ${MAKE_OPTS} MYCFLAGS=-fPIC linux
    make ${MAKE_OPTS} MYCFLAGS=-fPIC INSTALL_TOP=${PREFIX} install
    popd
fi

# Build & Install mosquitto
if ! pkg-config "libmosquitto >= 1.4.15" && \
    should_build "mosquitto" "for the MQTT node-type"; then
    git clone ${GIT_OPTS} --branch v2.0.15 https://github.com/eclipse/mosquitto.git
    mkdir -p mosquitto/build
    pushd mosquitto/build
    cmake -DWITH_BROKER=OFF \
          -DWITH_CLIENTS=OFF \
          -DWITH_APPS=OFF \
          -DDOCUMENTATION=OFF \
          ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} install
    popd
fi

# Build & Install rabbitmq-c
if ! pkg-config "librabbitmq >= 0.13.0" && \
    should_build "rabbitmq" "for the AMQP node and VILLAScontroller"; then
    git clone ${GIT_OPTS} --branch v0.11.0 https://github.com/alanxz/rabbitmq-c.git
    mkdir -p rabbitmq-c/build
    pushd rabbitmq-c/build
    cmake ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} install
    popd
fi

# Build & Install libzmq
if ! pkg-config "libzmq >= 2.2.0" && \
    should_build "zmq" "for the zeromq node-type"; then
    git clone ${GIT_OPTS} --branch v4.3.4 https://github.com/zeromq/libzmq.git
    mkdir -p libzmq/build
    pushd libzmq/build
    cmake -DWITH_PERF_TOOL=OFF \
          -DZMQ_BUILD_TESTS=OFF \
          -DENABLE_CPACK=OFF \
          ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} install
    popd
fi

# Build & Install EtherLab
if ! pkg-config "libethercat >= 1.5.2" && \
    should_build "ethercat" "for the ethercat node-type"; then
    git clone ${GIT_OPTS} --branch stable-1.5 https://gitlab.com/etherlab.org/ethercat.git
    pushd ethercat
    ./bootstrap
    ./configure --enable-userlib=yes --enable-kernel=no ${CONFIGURE_OPTS}
     make ${MAKE_OPTS} install
    popd
fi

# Build & Install libiec61850
if ! pkg-config "libiec61850 >= 1.5.0" && \
    should_build "iec61850" "for the iec61850 node-type"; then
    git clone ${GIT_OPTS} --branch v1.5.1 https://github.com/mz-automation/libiec61850.git
    mkdir -p libiec61850/build
    pushd libiec61850/build
    cmake -DBUILD_EXAMPLES=OFF \
          -DBUILD_PYTHON_BINDINGS=OFF \
          ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} install
    popd
fi

# Build & Install lib60870
if ! pkg-config "lib60870 >= 2.3.1" && \
    should_build "iec60870" "for the iec60870 node-type"; then
    git clone ${GIT_OPTS} --branch v2.3.2 https://github.com/mz-automation/lib60870.git
    mkdir -p lib60870/build
    pushd lib60870/build
    cmake -DBUILD_EXAMPLES=OFF \
          -DBUILD_TESTS=OFF \
          ${CMAKE_OPTS} ../lib60870-C
    make ${MAKE_OPTS} install
    popd
fi

# Build & Install librdkafka
if ! pkg-config "rdkafka >= 1.5.0" && \
    should_build "rdkafka" "for the kafka node-type"; then
    git clone ${GIT_OPTS} --branch v2.0.1 https://github.com/edenhill/librdkafka.git
    mkdir -p librdkafka/build
    pushd librdkafka/build
    cmake -DRDKAFKA_BUILD_TESTS=OFF \
          -DRDKAFKA_BUILD_EXAMPLES=OFF \
          -DWITH_CURL=OFF \
          ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} install
    popd
fi

# Build & Install Graphviz
if ! ( pkg-config "libcgraph >= 2.30" && \
       pkg-config "libgvc >= 2.30" \
     ) && should_build "graphviz" "for villas-graph"; then
    git clone ${GIT_OPTS} --branch 2.50.0 https://gitlab.com/graphviz/graphviz.git
    mkdir -p graphviz/build
    pushd graphviz/build
    cmake ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} install
    popd
fi

# Build & Install uldaq
if ! pkg-config "libuldaq >= 1.2.0" && \
    should_build "uldaq" "for the uldaq node-type"; then
    git clone ${GIT_OPTS} --branch v1.2.1 https://github.com/mccdaq/uldaq.git
    pushd uldaq
    autoreconf -i
    ./configure \
        --disable-examples \
        ${CONFIGURE_OPTS}
    make ${MAKE_OPTS} install
    popd
fi

# Build & Install libnl3
if ! ( pkg-config "libnl-3.0 >= 3.2.25" && \
       pkg-config "libnl-route-3.0 >= 3.2.25" \
     ) && should_build "libnl" "for network emulation"; then
    git clone ${GIT_OPTS} --branch libnl3_7_0 https://github.com/thom311/libnl.git
    pushd libnl
    autoreconf -i
    ./configure \
        --enable-cli=no \
        ${CONFIGURE_OPTS}
    make ${MAKE_OPTS} install
    popd
fi

# Build & Install libconfig
if ! pkg-config "libconfig >= 1.4.9" && \
    should_build "libconfig" "for libconfig configuration syntax"; then
    git clone ${GIT_OPTS} --branch v1.7.3 https://github.com/hyperrealm/libconfig.git
    pushd libconfig
    autoreconf -i
    ./configure ${CONFIGURE_OPTS} \
        --disable-tests \
        --disable-examples \
        --disable-doc
    make ${MAKE_OPTS} install
    popd
fi

# Build & Install comedilib
if ! pkg-config "comedilib >= 0.11.0" && \
    should_build "comedi" "for the comedi node-type"; then
    git clone ${GIT_OPTS} --branch r0_12_0 https://github.com/Linux-Comedi/comedilib.git
    pushd comedilib
    ./autogen.sh
    ./configure \
        --disable-docbook \
        ${CONFIGURE_OPTS}
    make ${MAKE_OPTS} install
    popd
fi

# Build & Install libre
if ! pkg-config "libre >= 2.9.0" && \
    should_build "libre" "for the rtp node-type"; then
    git clone ${GIT_OPTS} --branch v2.9.0 https://github.com/baresip/re.git
    pushd re
    make ${MAKE_OPTS} install
    popd
fi

# Build & Install nanomsg
if ! pkg-config "nanomsg >= 1.0.0" && \
    should_build "nanomsg" "for the nanomsg node-type"; then
    git clone ${GIT_OPTS} --branch 1.2 https://github.com/nanomsg/nanomsg.git
    mkdir -p nanomsg/build
    pushd nanomsg/build
    cmake -DNN_TESTS=OFF \
          -DNN_TOOLS=OFF \
          -DNN_STATIC_LIB=OFF \
          -DNN_ENABLE_DOC=OFF \
          -DNN_ENABLE_COVERAGE=OFF \
          ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} install
    popd
fi

# Build & Install libxil
if ! pkg-config "libxil >= 1.0.0" && \
    should_build "libxil" "for the fpga node-type"; then
    git clone ${GIT_OPTS} --branch master https://git.rwth-aachen.de/acs/public/villas/fpga/libxil.git
    mkdir -p libxil/build
    pushd libxil/build
    cmake ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} install
    popd
fi

# Build & Install hiredis
if ! pkg-config "hiredis >= 1.0.0" && \
    should_build "hiredis" "for the redis node-type"; then
    git clone ${GIT_OPTS} --branch v1.1.0 https://github.com/redis/hiredis.git
    mkdir -p hiredis/build
    pushd hiredis/build
    cmake -DDISABLE_TESTS=ON \
          -DENABLE_SSL=ON \
          ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} install
    popd
fi

# Build & Install redis++
if ! pkg-config "redis++ >= 1.2.3" && \
    should_build "redis++" "for the redis node-type"; then
    git clone ${GIT_OPTS} --branch 1.3.7 https://github.com/sewenew/redis-plus-plus.git
    mkdir -p redis-plus-plus/build
    pushd redis-plus-plus/build

    # Somehow redis++ fails to find the hiredis include path on Debian multiarch builds
    REDISPP_CMAKE_OPTS+="-DCMAKE_CXX_FLAGS=-I${PREFIX}/include"

    cmake -DREDIS_PLUS_PLUS_BUILD_TEST=OFF \
          -DREDIS_PLUS_PLUS_BUILD_STATIC=OFF \
          -DREDIS_PLUS_PLUS_CXX_STANDARD=17 \
          ${REDISPP_CMAKE_OPTS} ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} install VERBOSE=1
    popd
fi

# Build & Install Fmtlib
if ! pkg-config "fmt >= 6.1.2" && \
    should_build "fmt" "for logging" "required"; then
    git clone ${GIT_OPTS} --branch 6.1.2 --recursive https://github.com/fmtlib/fmt.git
    mkdir -p fmt/build
    pushd fmt/build
    cmake -DBUILD_SHARED_LIBS=1 \
          -DFMT_TEST=OFF \
          ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} install
    popd
fi

# Build & Install spdlog
if ! pkg-config "spdlog >= 1.8.2" && \
    should_build "spdlog" "for logging" "required"; then
    git clone ${GIT_OPTS} --branch v1.8.2 --recursive https://github.com/gabime/spdlog.git
    mkdir -p spdlog/build
    pushd spdlog/build
    cmake -DSPDLOG_FMT_EXTERNAL=ON \
          -DSPDLOG_BUILD_BENCH=OFF \
          -DSPDLOG_BUILD_SHARED=ON \
          -DSPDLOG_BUILD_TESTS=OFF \
          ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} install
    popd
fi

# Build & Install libwebsockets
if ! pkg-config "libwebsockets >= 4.3.0" && \
    should_build "libwebsockets" "for the websocket node and VILLASweb" "required"; then
    git clone ${GIT_OPTS} --branch v4.3-stable https://github.com/warmcat/libwebsockets.git
    mkdir -p libwebsockets/build
    pushd libwebsockets/build
    cmake -DLWS_WITH_IPV6=ON \
          -DLWS_WITHOUT_TESTAPPS=ON \
          -DLWS_WITHOUT_EXTENSIONS=OFF \
          ${CMAKE_OPTS} ..
    make ${MAKE_OPTS} install
    popd
fi

# Build & Install libdatachannel
if ! cmake --find-package -DNAME=LibDataChannel -DCOMPILER_ID=GNU -DLANGUAGE=CXX -DMODE=EXIST >/dev/null 2>/dev/null && \
    should_build "libdatachannel" "for the webrtc node-type"; then
    git clone ${GIT_OPTS} --recursive --branch v0.18.4 https://github.com/paullouisageneau/libdatachannel.git
    mkdir -p libdatachannel/build
    pushd libdatachannel/build

    if pkg-config "nice >= 0.1.16"; then
        CMAKE_DATACHANNEL_USE_NICE=-DUSE_NICE=ON
    fi

    cmake -DNO_MEDIA=ON \
          -DNO_WEBSOCKET=ON \
          ${CMAKE_DATACHANNEL_USE_NICE-} \
          ${CMAKE_OPTS} ..

    make ${MAKE_OPTS} install
    popd
fi

# Build & Install libmodbus
if ! pkg-config "libmodbus >= 3.1.0" && \
    should_build "libmodbus" "for the modbus node-type"; then
    git clone ${GIT_OPTS} --recursive --branch v3.1.10 https://github.com/stephane/libmodbus.git
    mkdir -p libmodbus/build
    pushd libmodbus
    autoreconf -i
    ./configure ${CONFIGURE_OPTS}
    make ${MAKE_OPTS} install
    popd
fi

popd >/dev/null
rm -rf ${TMPDIR}

# Update linker cache
if [ -z "${SKIP_LDCONFIG+x}${DEPS_SCAN+x}" ]; then
    ldconfig
fi
