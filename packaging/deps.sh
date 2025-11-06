#!/usr/bin/env bash
#
# A shell script to install various dependencies required by VILLASnode
#
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

has_command() {
    command -v "$1" >/dev/null 2>&1
}

check_cmake_version() {
    local cmake_version
    cmake_version=$(cmake --version | head -n1 | awk '{print $3}')
    local required_version=$1

    if [ "$(printf '%s\n%s\n' "$required_version" "$cmake_version" | sort -V | head -n1)" = "$required_version" ]; then
        return 0
    else
        return 1
    fi
}

## Build configuration

# Use shallow git clones to speed up downloads
GIT_OPTS+=" --depth=1 --recurse-submodules --shallow-submodules --config advice.detachedHead=false"

# Install destination
PREFIX=${PREFIX:-/usr/local}
if [[ "${PREFIX}" == "/usr/local" ]]; then
    PIP_PREFIX="$(pwd)/venv"
    PATH="${PIP_PREFIX}/bin:${PREFIX}/bin:${PATH}"
else
    PIP_PREFIX="${PREFIX}"
    PATH="${PREFIX}/bin:${PATH}"
fi

# Cross-compile
TRIPLET=${TRIPLET:-$(gcc -dumpmachine)}
ARCH=${ARCH:-$(uname -m)}

# CMake
CMAKE_OPTS+=" -Wno-dev -G=Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${PREFIX}"

# Autotools
CONFIGURE_OPTS+=" --host=${TRIPLET} --prefix=${PREFIX}"

# Make
PARALLEL=${PARALLEL:-$(nproc)}
MAKE_OPTS+=" -j ${PARALLEL}"

# Ninja
NINJA_OPTS+=" -j ${PARALLEL}"

# pkg-config
PKG_CONFIG_PATH=${PKG_CONFIG_PATH:-}${PKG_CONFIG_PATH:+:}${PREFIX}/lib/pkgconfig:${PREFIX}/lib64/pkgconfig:${PREFIX}/share/pkgconfig
export PKG_CONFIG_PATH

SOURCE_DIR=$(realpath $(dirname "${BASH_SOURCE[0]}"))

# Build in a temporary directory
TMPDIR=$(mktemp -d)
trap "rm -rf ${TMPDIR}" EXIT

echo "Entering ${TMPDIR}"
pushd ${TMPDIR} >/dev/null

# Check for pkg-config
if ! has_command pkg-config; then
    echo -e "Error: pkg-config is required to check for existing dependencies."
    exit 1
fi

# Enter python venv
python3 -m venv ${PIP_PREFIX}
. ${PIP_PREFIX}/bin/activate

# Install some build-tools
python3 -m pip install \
    --prefix=${PIP_PREFIX} \
    --upgrade \
    pip \
    setuptools \

python3 -m pip install \
    --prefix=${PIP_PREFIX} \
    cmake==3.31.6 \
    meson==1.9.1 \
    ninja==1.11.1.4

# Build & Install Criterion
if ! pkg-config "criterion >= 2.4.1" && \
   [ "${ARCH}" == "x86_64" ] && \
   should_build "criterion" "for unit tests"; then
    git clone ${GIT_OPTS} --branch v2.4.3 --recursive https://github.com/Snaipe/Criterion.git
    pushd Criterion

    meson setup \
        --prefix=${PREFIX} \
        --cmake-prefix-path=${PREFIX} \
        --backend=ninja \
        build
    meson compile -C build
    meson install -C build

    popd
fi

# Build & Install jansson
if ! pkg-config "jansson >= 2.13" && \
    should_build "jansson" "for configuration parsing" "required"; then
    git clone ${GIT_OPTS} --branch v2.14.1 https://github.com/akheron/jansson.git
    mkdir -p jansson/build
    pushd jansson/build
    cmake -DJANSSON_BUILD_SHARED_LIBS=ON \
          -DJANSSON_BUILD_STATIC_LIBS=OFF \
          -DJANSSON_WITHOUT_TESTS=ON \
          -DJANSSON_EXAMPLES=OFF \
          ${CMAKE_OPTS} ..
    cmake --build . \
        --target install \
        --parallel ${PARALLEL}
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
    curl -L http://www.lua.org/ftp/lua-5.4.7.tar.gz | tar -xz
    pushd lua-5.4.7
    make ${MAKE_OPTS} MYCFLAGS=-fPIC linux
    make ${MAKE_OPTS} MYCFLAGS=-fPIC INSTALL_TOP=${PREFIX} install
    popd
fi

# Build & Install mosquitto
if ! pkg-config "libmosquitto >= 1.4.15" && \
    should_build "mosquitto" "for the mqtt node-type"; then
    git clone ${GIT_OPTS} --branch v2.0.22 https://github.com/eclipse/mosquitto.git
    mkdir -p mosquitto/build
    pushd mosquitto/build
    cmake -DWITH_BROKER=OFF \
          -DWITH_CLIENTS=OFF \
          -DWITH_APPS=OFF \
          -DDOCUMENTATION=OFF \
          ${CMAKE_OPTS} ..
    cmake --build . \
        --target install \
        --parallel ${PARALLEL}
    popd
fi

# Build & Install rabbitmq-c
if ! pkg-config "librabbitmq >= 0.13.0" && \
    should_build "rabbitmq" "for the amqp node-node and VILLAScontroller"; then
    git clone ${GIT_OPTS} --branch v0.15.0 https://github.com/alanxz/rabbitmq-c.git
    mkdir -p rabbitmq-c/build
    pushd rabbitmq-c/build
    cmake ${CMAKE_OPTS} ..
    cmake --build . \
        --target install \
        --parallel ${PARALLEL}
    popd
fi

# Build & Install libzmq
if ! pkg-config "libzmq >= 2.2.0" && \
    should_build "zmq" "for the zeromq node-type"; then
    git clone ${GIT_OPTS} --branch v4.3.5 https://github.com/zeromq/libzmq.git
    mkdir -p libzmq/build
    pushd libzmq/build
    cmake -DWITH_PERF_TOOL=OFF \
          -DZMQ_BUILD_TESTS=OFF \
          -DENABLE_CPACK=OFF \
          ${CMAKE_OPTS} ..
    cmake --build . \
        --target install \
        --parallel ${PARALLEL}
    popd
fi

# Build & Install EtherLab
if ! pkg-config "libethercat >= 1.5.2" && \
    should_build "ethercat" "for the ethercat node-type"; then
    git clone ${GIT_OPTS} --branch 1.6.8 https://gitlab.com/etherlab.org/ethercat.git
    pushd ethercat
    ./bootstrap
    ./configure --enable-userlib=yes --enable-kernel=no ${CONFIGURE_OPTS}
     make ${MAKE_OPTS} install
    popd
fi

# Build & Install libiec61850
if ! pkg-config "libiec61850 >= 1.6.0" && \
    should_build "iec61850" "for the iec61850 node-type"; then
    git clone ${GIT_OPTS} --branch v1.6.1 https://github.com/mz-automation/libiec61850.git

    pushd libiec61850/third_party/mbedtls/
    curl -L https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/v3.6.0.tar.gz | tar -xz
    popd

    mkdir -p libiec61850/build
    pushd libiec61850/build
    cmake -DBUILD_EXAMPLES=OFF \
          -DBUILD_PYTHON_BINDINGS=OFF \
          ${CMAKE_OPTS} ..
    cmake --build . \
        --target install \
        --parallel ${PARALLEL}
    popd
fi

# Build & Install lib60870
if ! pkg-config "lib60870 >= 2.3.1" && \
    should_build "iec60870" "for the iec60870 node-type"; then
    git clone ${GIT_OPTS} --branch v2.3.6 https://github.com/mz-automation/lib60870.git
    mkdir -p lib60870/build
    pushd lib60870/build
    cmake -DBUILD_EXAMPLES=OFF \
          -DBUILD_TESTS=OFF \
          ${CMAKE_OPTS} ../lib60870-C
    cmake --build . \
        --target install \
        --parallel ${PARALLEL}
    popd
fi

# Build & Install librdkafka
if ! pkg-config "rdkafka >= 1.5.0" && \
    should_build "rdkafka" "for the kafka node-type"; then
    git clone ${GIT_OPTS} --branch v2.12.1 https://github.com/edenhill/librdkafka.git
    mkdir -p librdkafka/build
    pushd librdkafka/build
    cmake -DRDKAFKA_BUILD_TESTS=OFF \
          -DRDKAFKA_BUILD_EXAMPLES=OFF \
          -DWITH_CURL=OFF \
          ${CMAKE_OPTS} ..
    cmake --build . \
        --target install \
        --parallel ${PARALLEL}
    popd
fi

# Build & Install Graphviz
if ! ( pkg-config "libcgraph >= 2.30" && \
       pkg-config "libgvc >= 2.30" \
     ) && should_build "graphviz" "for villas-graph"; then
    curl -L https://gitlab.com/api/v4/projects/4207231/packages/generic/graphviz-releases/14.0.2/graphviz-14.0.2.tar.gz | tar -xz
    mkdir -p graphviz-14.0.2
    pushd graphviz-14.0.2
    ./configure --enable-shared ${CONFIGURE_OPTS}
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

# Build & Install libnl
if ! ( pkg-config "libnl-3.0 >= 3.2.25" && \
       pkg-config "libnl-route-3.0 >= 3.2.25" \
     ) && should_build "libnl" "for network emulation"; then
    git clone ${GIT_OPTS} --branch libnl3_11_0 https://github.com/thom311/libnl.git
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
    git clone ${GIT_OPTS} --branch v1.8.1 https://github.com/hyperrealm/libconfig.git
    mkdir -p libconfig/build
    pushd libconfig/build
    cmake -DBUILD_EXAMPLES=OFF \
          -DBUILD_TESTS=OFF \
          -DBUILD_CXX=OFF \
          ${CMAKE_OPTS} ..
    cmake --build . \
        --target install \
        --parallel ${PARALLEL}
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
if ! pkg-config "libre >= 3.6.0" && \
    should_build "libre" "for the rtp node-type"; then
    git clone ${GIT_OPTS} --branch v4.2.0 https://github.com/baresip/re.git
    mkdir -p re/build
    pushd re/build
    cmake -DUSE_BFCP=OFF \
          -DUSE_PCP=OFF \
          -DUSE_RTMP=OFF \
          -DUSE_SIP=OFF \
          -DLIBRE_BUILD_STATIC=OFF  \
          ${CMAKE_OPTS} ..
    cmake --build . \
        --target install \
        --parallel ${PARALLEL}
    popd
fi

# Build & Install nanomsg
if ! pkg-config "nanomsg >= 1.0.0" && \
    should_build "nanomsg" "for the nanomsg node-type"; then
    # TODO: v1.2.1 seems to be broken: https://github.com/nanomsg/nanomsg/issues/1111
    git clone ${GIT_OPTS} --branch 1.2.2 https://github.com/nanomsg/nanomsg.git
    mkdir -p nanomsg/build
    pushd nanomsg/build
    cmake -DNN_TESTS=OFF \
          -DNN_TOOLS=OFF \
          -DNN_STATIC_LIB=OFF \
          -DNN_ENABLE_DOC=OFF \
          -DNN_ENABLE_COVERAGE=OFF \
          ${CMAKE_OPTS} ..
    cmake --build . \
        --target install \
        --parallel ${PARALLEL}
    popd
fi

# Build & Install libxil
if ! pkg-config "libxil >= 1.0.0" && \
    should_build "libxil" "for the fpga node-type"; then
    git clone ${GIT_OPTS} --branch master https://github.com/VILLASframework/libxil.git
    mkdir -p libxil/build
    pushd libxil/build
    cmake ${CMAKE_OPTS} ..
    cmake --build . \
        --target install \
        --parallel ${PARALLEL}
    popd
fi

# Build & Install hiredis
if ! pkg-config "hiredis >= 1.0.0" && \
    should_build "hiredis" "for the redis node-type"; then
    git clone ${GIT_OPTS} --branch v1.3.0 https://github.com/redis/hiredis.git
    mkdir -p hiredis/build
    pushd hiredis/build
    cmake -DDISABLE_TESTS=ON \
          -DENABLE_SSL=ON \
          ${CMAKE_OPTS} ..
    cmake --build . \
        --target install \
        --parallel ${PARALLEL}
    popd
fi

# Build & Install redis++
if ! pkg-config "redis++ >= 1.2.3" && \
    should_build "redis++" "for the redis node-type"; then
    git clone ${GIT_OPTS} --branch 1.3.15 https://github.com/sewenew/redis-plus-plus.git
    mkdir -p redis-plus-plus/build
    pushd redis-plus-plus/build

    # Somehow redis++ fails to find the hiredis include path on Debian multiarch builds
    REDISPP_CMAKE_OPTS+="-DCMAKE_CXX_FLAGS=-I${PREFIX}/include"

    cmake -DREDIS_PLUS_PLUS_BUILD_TEST=OFF \
          -DREDIS_PLUS_PLUS_BUILD_STATIC=OFF \
          -DREDIS_PLUS_PLUS_CXX_STANDARD=17 \
          ${REDISPP_CMAKE_OPTS} ${CMAKE_OPTS} ..
    cmake --build . \
        --target install \
        --parallel ${PARALLEL}
    popd
fi

# Build & Install Fmtlib
if ! pkg-config "fmt >= 6.1.2" && \
    should_build "fmt" "for logging" "required"; then
    git clone ${GIT_OPTS} --branch 12.1.0 --recursive https://github.com/fmtlib/fmt.git
    mkdir -p fmt/build
    pushd fmt/build
    cmake -DBUILD_SHARED_LIBS=1 \
          -DFMT_TEST=OFF \
          ${CMAKE_OPTS} ..
    cmake --build . \
        --target install \
        --parallel ${PARALLEL}
    popd
fi

# Build & Install spdlog
if ! pkg-config "spdlog >= 1.8.2" && \
    should_build "spdlog" "for logging" "required"; then
    git clone ${GIT_OPTS} --branch v1.16.0 --recursive https://github.com/gabime/spdlog.git
    mkdir -p spdlog/build
    pushd spdlog/build
    cmake -DSPDLOG_FMT_EXTERNAL=ON \
          -DSPDLOG_BUILD_BENCH=OFF \
          -DSPDLOG_BUILD_SHARED=ON \
          -DSPDLOG_BUILD_TESTS=OFF \
          ${CMAKE_OPTS} ..
    cmake --build . \
        --target install \
        --parallel ${PARALLEL}
    popd
fi

# Build & Install libwebsockets
if ! pkg-config "libwebsockets >= 4.3.0" && \
    should_build "libwebsockets" "for the websocket node and VILLASweb" "required"; then
    git clone ${GIT_OPTS} --branch v4.3.6 https://github.com/warmcat/libwebsockets.git
    mkdir -p libwebsockets/build
    pushd libwebsockets/build
    cmake -DLWS_WITH_IPV6=ON \
          -DLWS_WITHOUT_TESTAPPS=ON \
          -DLWS_WITHOUT_EXTENSIONS=OFF \
          ${CMAKE_OPTS} ..
    cmake --build . \
        --target install \
        --parallel ${PARALLEL}
    popd
fi

# Build & Install libnice
if ! pkg-config "nice >= 0.1.16" && \
    should_build "libnice" "for the webrtc node-type"; then
    git clone ${GIT_OPTS} --branch 0.1.22 https://gitlab.freedesktop.org/libnice/libnice.git
    mkdir -p libnice/build
    pushd libnice

    meson setup \
        --prefix=${PREFIX} \
        --cmake-prefix-path=${PREFIX} \
        --backend=ninja \
        build
    meson compile -C build
    meson install -C build

    popd
fi

# Build & Install libdatachannel
if ! cmake --find-package -DNAME=LibDataChannel -DCOMPILER_ID=GNU -DLANGUAGE=CXX -DMODE=EXIST >/dev/null 2>/dev/null && \
    should_build "libdatachannel" "for the webrtc node-type"; then
    git clone ${GIT_OPTS} --recursive --branch v0.23.2 https://github.com/paullouisageneau/libdatachannel.git
    mkdir -p libdatachannel/build
    pushd libdatachannel/build

    if pkg-config "nice >= 0.1.16"; then
        CMAKE_DATACHANNEL_USE_NICE=-DUSE_NICE=ON
    fi

    cmake -DNO_MEDIA=ON \
          -DNO_WEBSOCKET=ON \
          ${CMAKE_DATACHANNEL_USE_NICE-} \
          ${CMAKE_OPTS} ..
    cmake --build . \
        --target install \
        --parallel ${PARALLEL}
    popd
fi

# Build & Install libmodbus
if ! pkg-config "libmodbus >= 3.1.0" && \
    should_build "libmodbus" "for the modbus node-type"; then
    git clone ${GIT_OPTS} --recursive --branch v3.1.11 https://github.com/stephane/libmodbus.git
    mkdir -p libmodbus/build
    pushd libmodbus
    autoreconf -i
    ./configure ${CONFIGURE_OPTS}
    make ${MAKE_OPTS} install
    popd
fi

# Build & Install OpenDSS
if ! find /usr/{local/,}{lib,bin} -name "libOpenDSSC.so" | grep -q . &&
    should_build "opendss" "For opendss node-type" &&
    has_command git-svn; then
    git svn clone -r 4020:4020 https://svn.code.sf.net/p/electricdss/code/trunk/VersionC OpenDSS-C
    mkdir -p OpenDSS-C/build
    pushd OpenDSS-C
    for i in ${SOURCE_DIR}/patches/*-opendssc-*.patch; do patch --strip=1 --binary < "$i"; done
    popd
    pushd OpenDSS-C/build
    if command -v g++-14 2>&1 >/dev/null; then
        # OpenDSS rev 4020 is not compatible with GCC 15
        OPENDSS_CMAKE_OPTS="-DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14"
    else
        OPENDSS_CMAKE_OPTS=""
    fi
    cmake -DMyOutputType=DLL \
          ${OPENDSS_CMAKE_OPTS} \
          ${CMAKE_OPTS} ..
    cmake --build . \
        --target install \
        --parallel ${PARALLEL}
    popd
fi

# Build & Install ghc::filesystem
if ! cmake --find-package -DNAME=ghc_filesystem -DCOMPILER_ID=GNU -DLANGUAGE=CXX -DMODE=EXIST >/dev/null 2>/dev/null && \
    should_build "ghc_filesystem" "for compatability with older compilers"; then
    git clone ${GIT_OPTS} --branch v1.5.14 https://github.com/gulrak/filesystem.git
    mkdir -p filesystem/build
    pushd filesystem/build
    cmake -DGHC_FILESYSTEM_BUILD_TESTING=OFF \
          -DGHC_FILESYSTEM_BUILD_EXAMPLES=OFF \
          ${CMAKE_OPTS} ..
    cmake --build . \
        --target install \
        --parallel ${PARALLEL}
    popd
fi

popd >/dev/null

# Update linker cache
if [ -z "${SKIP_LDCONFIG+x}${DEPS_SCAN+x}" ]; then
    ldconfig
fi
