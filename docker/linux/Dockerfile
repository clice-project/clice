# build with multi-stage for cache efficiency
# base image
FROM ubuntu:24.04 AS base

# tool chain versions will be read from config/default-toolchain-version

# disable apt-get interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# install basic tools
COPY config/default-toolchain-version /clice/config/default-toolchain-version

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    # TODO: support more cache for python, xmake installation
    # TODO: check why cache doesn't work after add-apt-repository, may we change it to cache?
    # TODO: check why set -e doesn't work here
    set -e; \
    rm -f /etc/apt/apt.conf.d/docker-clean; echo 'Binary::apt::APT::Keep-Downloaded-Packages "true";' > /etc/apt/apt.conf.d/keep-cache; \
    apt update; \
    apt install -y --no-install-recommends software-properties-common curl wget gpg git; \
    # python PPA
    add-apt-repository ppa:deadsnakes/ppa; \
    # cmake PPA from kitware
    wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc | gpg --dearmor -o /etc/apt/trusted.gpg.d/kitware.gpg; \
    CODENAME=$(lsb_release -cs 2>/dev/null || (source /etc/os-release && echo "$VERSION_CODENAME")); \
    add-apt-repository "deb https://apt.kitware.com/ubuntu/ ${CODENAME} main"; \
    apt update; \
    XMAKE_VERSION=$(grep -E '^xmake,' /clice/config/default-toolchain-version | cut -d',' -f2); \
    CMAKE_VERSION=$(grep -E '^cmake,' /clice/config/default-toolchain-version | cut -d',' -f2); \
    PYTHON_VERSION=$(grep -E '^python,' /clice/config/default-toolchain-version | cut -d',' -f2); \
    # cmake from kitware, python from deadsnakes, git, python
    apt install -y "cmake=${CMAKE_VERSION}-0kitware1ubuntu24.04.1" "cmake-data=${CMAKE_VERSION}-0kitware1ubuntu24.04.1" "python${PYTHON_VERSION}" "python3-pytest" "python3-pytest-asyncio"; \
    # xmake
    curl -fsSL https://xmake.io/shget.text | bash -s "v${XMAKE_VERSION}"; \
    echo "PATH=/root/.local/bin:${PATH}" >> ~/.bashrc;

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
    if [ "$COMPILER" = "gcc" ]; then \
        GCC_VERSION=$(grep -E '^gcc,' /clice/config/default-toolchain-version | cut -d',' -f2); \
        apt install -y "g++-${GCC_VERSION}"; \
    elif [ "$COMPILER" = "clang" ]; then \
        CLANG_VERSION=$(grep -E '^clang,' /clice/config/default-toolchain-version | cut -d',' -f2); \
        apt install -y "clang-${CLANG_VERSION}" "lld-${CLANG_VERSION}"; \
        update-alternatives --install /usr/bin/clang clang "/usr/bin/clang-${CLANG_VERSION}" 100; \
        update-alternatives --install /usr/bin/c++ c++ "/usr/bin/clang-${CLANG_VERSION}" 100; \
        update-alternatives --install /usr/bin/cc cc "/usr/bin/clang-${CLANG_VERSION}" 100; \
        update-alternatives --install /usr/bin/ld ld "/usr/bin/lld-${CLANG_VERSION}" 100; \
        apt purge --autoremove -y gcc g++ build-essential; \
        apt autoremove -y; \
        echo "export CC=clang" >> ~/.bashrc; \
        echo "export CXX=clang++" >> ~/.bashrc; \
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
ARG LLVM_BINARY_BASE_URL="https://github.com/clice-io/llvm-binary/releases/download"

WORKDIR /clice

RUN mkdir -p /clice/build

COPY config/llvm-binary /clice/config/llvm-binary

# download llvm-binary based on LTO_TYPE and verify sha256 (download both Debug and Release variants)
# TODO: support cache for downloaded llvm-binary, by checking sha256 if match
RUN bash -eux - <<'BASH'
set -e

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
    DEST_PATH="/clice/build/${FILE_NAME}"
    echo "Downloading ${DOWNLOAD_URL} -> ${DEST_PATH}"
    wget -q -O "${DEST_PATH}" "${DOWNLOAD_URL}"
    echo "${SHA256_HASH} ${DEST_PATH}" | sha256sum --check || (echo "SHA256 mismatch for ${DEST_PATH}" >&2; exit 1); \
}

LIB_DEBUG_INFO=$(grep -i "Linux,Debug,${LTO_TYPE}" /clice/config/llvm-binary)
download_and_check "$LIB_DEBUG_INFO"
LIB_RELEASE_INFO=$(grep -i "Linux,Release,${LTO_TYPE}" /clice/config/llvm-binary)
download_and_check "$LIB_RELEASE_INFO"
BASH

FROM dependency-stage AS final

WORKDIR /clice

CMD ["/bin/bash"]