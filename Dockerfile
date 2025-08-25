
FROM debian:13 AS builder

RUN --mount=target=/var/lib/apt/lists,type=cache,sharing=locked \
    --mount=target=/var/cache/apt,type=cache,sharing=locked \
    rm -f /etc/apt/apt.conf.d/docker-clean && \
    apt-get update && apt-get install -y \
    ninja-build cmake build-essential curl gcc-14 g++-14

# https://apt.llvm.org/
RUN echo "deb http://apt.llvm.org/trixie/ llvm-toolchain-trixie main" >> /etc/apt/sources.list
RUN echo "deb http://apt.llvm.org/trixie/ llvm-toolchain-trixie-20 main" >> /etc/apt/sources.list
RUN echo "deb http://apt.llvm.org/trixie/ llvm-toolchain-trixie-21 main" >> /etc/apt/sources.list
RUN curl https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
RUN --mount=target=/var/lib/apt/lists,type=cache,sharing=locked \
    --mount=target=/var/cache/apt,type=cache,sharing=locked \
    rm -f /etc/apt/apt.conf.d/docker-clean && \
    apt-get update && apt-get install -y \
    clang-20

ADD include /app/include
ADD src /app/src
ADD tests /app/tests
ADD CMakeLists.txt /app/CMakeLists.txt
ADD scripts /app/scripts

WORKDIR /app
RUN ls -la scripts
RUN cp scripts/build.sh scripts/build-release.sh && sed -i 's/DCMAKE_BUILD_TYPE=Debug/DCMAKE_BUILD_TYPE=Release/g' scripts/build-release.sh
RUN bash -c ./scripts/build-release.sh

RUN cmake --install . --prefix=/app/clice

FROM debian:13 

COPY --from=builder /app/clice /app/clice

CMD ["/app/clice/bin/clice"]
