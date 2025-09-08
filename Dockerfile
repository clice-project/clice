FROM debian:13 AS dev-base

# Installs System Dependencies
RUN apt-get update && apt-get install -y \
    ninja-build cmake build-essential curl gcc-14 g++-14 git clang lld && \
    curl -LsSf https://astral.sh/uv/install.sh | sh && \
    curl -fsSL https://xmake.io/shget.text | bash

FROM dev-base AS builder

# Adds source code
COPY include /app/include
COPY cmake /app/cmake
COPY src /app/src
COPY tests /app/tests
COPY CMakeLists.txt /app/CMakeLists.txt
COPY scripts /app/scripts

WORKDIR /app
# Initializes and installs dependencies
RUN cmake -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release -DCLICE_ENABLE_TEST=ON
# Builds clice
RUN cmake --build build -j && \
    cmake --install build --prefix=/opt/clice

FROM debian:13

COPY --from=builder /opt/clice /opt/clice/
COPY LICENSE /opt/clice/LICENSE
COPY README.md /opt/clice/README.md

RUN ln -s /opt/clice/bin/clice /usr/local/bin/clice

ENTRYPOINT ["clice"]
