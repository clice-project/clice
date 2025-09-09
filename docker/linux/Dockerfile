# build with multi-stage for cache efficiency
# base image
FROM ubuntu:24.04 AS base

# tool chain versions will be read from config/default-toolchain-version
COPY config/default-toolchain-version /clice/config/default-toolchain-version
# python and dependencies config
COPY pyproject.toml /clice/pyproject.toml

# install basic tools
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    # TODO: support more cache for python, xmake installation
    # TODO: check why cache doesn't work after add-apt-repository, may we change it to cache?
bash -eux - <<'BASH'
install_common_requests() {
    set -e
    rm -f /etc/apt/apt.conf.d/docker-clean
    echo 'Binary::apt::APT::Keep-Downloaded-Packages "true";' > /etc/apt/apt.conf.d/keep-cache  
    XMAKE_VERSION=$(grep -E '^xmake,' /clice/config/default-toolchain-version | cut -d',' -f2)  
    CMAKE_VERSION=$(grep -E '^cmake,' /clice/config/default-toolchain-version | cut -d',' -f2)  
    PYTHON_VERSION=$(grep -E '^python,' /clice/config/default-toolchain-version | cut -d',' -f2)  
    # ensure user-local bin is on PATH for non-apt installs
    # for xmake, uv, python
    echo 'PATH=/root/.local/bin:${PATH}' >> ~/.bashrc  
    apt update  
    # first install minimal apt prerequisites (curl required, software-properties-common for add-apt-repository)
    apt install -y --no-install-recommends software-properties-common curl gpg 
}

install_xmake() {
    echo "Installing xmake ${XMAKE_VERSION}..." 
    curl -fsSL https://xmake.io/shget.text | bash -s "v${XMAKE_VERSION}"  
    echo "export XMAKE_ROOT=y" >> ~/.bashrc
}

install_python() {
    echo "Installing python ${PYTHON_VERSION} via uv..." 
    curl -LsSf https://astral.sh/uv/install.sh | sh  
    source $HOME/.local/bin/env  
    uv python install "${PYTHON_VERSION}"  
    uv sync
}

install_apt_packages() {
    # add cmake PPA from kitware (use curl instead of wget)
    curl -fsSL https://apt.kitware.com/keys/kitware-archive-latest.asc | gpg --dearmor -o /etc/apt/trusted.gpg.d/kitware.gpg  
    CODENAME=$(lsb_release -cs 2>/dev/null || (source /etc/os-release && echo "$VERSION_CODENAME"))  
    add-apt-repository "deb https://apt.kitware.com/ubuntu/ ${CODENAME} main"  
    apt update  
    set -e  
    # install cmake (and other apt deps) after repositories added
    apt install -y --no-install-recommends git "cmake=${CMAKE_VERSION}-0kitware1ubuntu24.04.1" "cmake-data=${CMAKE_VERSION}-0kitware1ubuntu24.04.1"
}

do_install() {
    set -e
    
    mkdir -p /clice
    
    cd /clice
    
    # pre-requests required by both apt and non-apt installers
    install_common_requests
    
    # run non-apt installers in parallel tasks: (1) xmake, (2) uv+python
    install_xmake & TASK_XMAKE=$!
    install_python & TASK_UV=$!
    
    # install apt packages (blocking)
    install_apt_packages
    
    # wait for background tasks and propagate their exit codes
    wait $TASK_XMAKE; XMAKE_STATUS=$?
    wait $TASK_UV; UV_STATUS=$? 
    if [ $XMAKE_STATUS -ne 0 ] || [ $UV_STATUS -ne 0 ]; then
        echo "One of parallel setup tasks failed (apt:$APT_STATUS uv:$UV_STATUS xmake:$XMAKE_STATUS)" >&2
        exit 1
    fi

    echo "Base image prepared"
}

do_install

BASH

# Compiler stage
FROM base AS compiler-stage

# passed from build arg
ARG COMPILER

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    # TODO: support more cache for gcc,clang installation
    set -e; \
    # read toolchain versions
    apt update; \
    apt install -y --no-install-recommends software-properties-common; \
    # gcc, llvm PPA
    add-apt-repository ppa:ubuntu-toolchain-r/ppa; \
    apt update; \
    apt purge --autoremove -y gcc g++ build-essential; \
    apt autoremove -y; \
    if [ "$COMPILER" = "gcc" ]; then \
        GCC_VERSION=$(grep -E '^gcc,' /clice/config/default-toolchain-version | cut -d',' -f2); \
        apt install -y "g++-${GCC_VERSION}"; \
        update-alternatives --install /usr/bin/c++ c++ "/usr/bin/g++-${GCC_VERSION}" 100; \
        update-alternatives --install /usr/bin/cc cc "/usr/bin/gcc-${GCC_VERSION}" 100; \
        update-alternatives --install /usr/bin/ld ld "/usr/bin/ld-${GCC_VERSION}" 100; \
    elif [ "$COMPILER" = "clang" ]; then \
    CLANG_VERSION=$(grep -E '^clang,' /clice/config/default-toolchain-version | cut -d',' -f2); \
    # try to install additional clang-related packages, fallback to minimal set if some aren't available
    apt install -y "clang-${CLANG_VERSION}" "clang-tools-${CLANG_VERSION}" "clang-format-${CLANG_VERSION}" "clang-tidy-${CLANG_VERSION}" "lld-${CLANG_VERSION}" libc++-dev libc++abi-dev || apt install -y "clang-${CLANG_VERSION}" "lld-${CLANG_VERSION}"; \
    update-alternatives --install /usr/bin/clang clang "/usr/bin/clang-${CLANG_VERSION}" 100; \
    update-alternatives --install /usr/bin/clang++ clang++ "/usr/bin/clang++-${CLANG_VERSION}" 100; \
    update-alternatives --install /usr/bin/c++ c++ "/usr/bin/clang++-${CLANG_VERSION}" 100; \
    update-alternatives --install /usr/bin/cc cc "/usr/bin/clang-${CLANG_VERSION}" 100; \
    update-alternatives --install /usr/bin/ld ld "/usr/bin/lld-${CLANG_VERSION}" 100; \
    echo "export CC=/usr/bin/clang-${CLANG_VERSION}" >> ~/.bashrc; \
    echo "export CXX=/usr/bin/clang++-${CLANG_VERSION}" >> ~/.bashrc; \
    echo "export LDFLAGS=-fuse-ld=lld" >> ~/.bashrc; \
    echo "export CXXFLAGS=-stdlib=libc++" >> ~/.bashrc; \
    else \
        echo "Error: Unsupported compiler '$COMPILER'. Use 'gcc' or 'clang'." >&2; exit 1; \
    fi

# download compile dependencies
FROM compiler-stage AS dependency-stage

# passed from build arg
# "lto" or "non_lto"
ARG LTO_TYPE
ARG CACHE_DIR="/var/cache/llvm-binary"
ARG LLVM_BINARY_BASE_URL="https://github.com/clice-io/llvm-binary/releases/download"

COPY config/llvm-binary /clice/config/llvm-binary

# download llvm-binary based on LTO_TYPE and verify sha256 (download both Debug and Release variants)
# Use buildx cache at /var/cache/llvm-binary to avoid re-downloading between builds.
RUN --mount=type=cache,target=${CACHE_DIR},sharing=locked \
bash -eux - <<'BASH'

download_and_check() {
    LINE="$1"
    if [ -z "$LINE" ]; then
        echo "Empty entry, skipping"
        return 1
    fi
    # parse CSV: PLATFORM,BUILD_TYPE,LTO_FIELD,FILE_NAME,LLVM_VERSION,SHA256_HASH
    IFS=','; set -- $LINE
    PLATFORM="$1"; BUILD_TYPE="$2"; LTO_FIELD="$3"; FILE_NAME="$4"; LLVM_VERSION="$5"; SHA256_HASH="$6"
    DOWNLOAD_URL="${LLVM_BINARY_BASE_URL}/${LLVM_VERSION}/${FILE_NAME}"
    DEST_PATH="/llvm-binary/${FILE_NAME}"
    CACHE_PATH="${CACHE_DIR}/${FILE_NAME}"

    mkdir -p "${CACHE_DIR}" "$(dirname "${DEST_PATH}")"

    # If cached file exists and matches hash, copy from cache and skip download
    if [ -f "${CACHE_PATH}" ]; then
        echo "Found cached ${CACHE_PATH}, verifying sha256..."
        if echo "${SHA256_HASH}  ${CACHE_PATH}" | sha256sum --check >/dev/null 2>&1; then
            echo "SHA256 OK in cache, copying ${FILE_NAME} to ${DEST_PATH}"
            cp -a "${CACHE_PATH}" "${DEST_PATH}"
            return 0
        else
            echo "Cached file ${CACHE_PATH} failed SHA256, will re-download" >&2
            rm -f "${CACHE_PATH}"
        fi
    fi

    echo "Downloading ${DOWNLOAD_URL} -> ${CACHE_PATH} (cache)"
    curl -fsSL -o "${CACHE_PATH}" "${DOWNLOAD_URL}" || (echo "Download failed for ${DOWNLOAD_URL}" >&2; return 1)
    echo "${SHA256_HASH}  ${CACHE_PATH}" | sha256sum --check || (echo "SHA256 mismatch for ${CACHE_PATH}" >&2; exit 1)
    cp -a "${CACHE_PATH}" "${DEST_PATH}"
}

set -e

mkdir -p /clice/build /llvm-binary

cd /clice

LIB_DEBUG_INFO=$(grep -i "Linux,Debug,${LTO_TYPE}" /clice/config/llvm-binary || true)
LIB_RELEASE_INFO=$(grep -i "Linux,Release,${LTO_TYPE}" /clice/config/llvm-binary || true)

# start downloads in background to parallelize network I/O
if [ -n "$LIB_DEBUG_INFO" ]; then
    download_and_check "$LIB_DEBUG_INFO" & TASK_DEBUG=$!
else
    TASK_DEBUG=0; DEBUG_STATUS=0
fi

if [ -n "$LIB_RELEASE_INFO" ]; then
    download_and_check "$LIB_RELEASE_INFO" & TASK_RELEASE=$!
else
    TASK_RELEASE=0; RELEASE_STATUS=0
fi

if [ "$TASK_DEBUG" -ne 0 ]; then
    wait $TASK_DEBUG; DEBUG_STATUS=$?
fi
if [ "$TASK_RELEASE" -ne 0 ]; then
    wait $TASK_RELEASE; RELEASE_STATUS=$?
fi
if [ $DEBUG_STATUS -ne 0 ] || [ $RELEASE_STATUS -ne 0 ]; then
    echo "One of the downloads failed (Debug:$DEBUG_STATUS Release:$RELEASE_STATUS)" >&2;
    exit 1
fi

BASH

FROM dependency-stage AS final

WORKDIR /clice

CMD ["/bin/bash"]