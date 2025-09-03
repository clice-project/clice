
FROM debian:13 AS builder

# Installing System Dependencies
RUN apt-get update && apt-get install -y \
    ninja-build cmake build-essential curl gcc-14 g++-14 git

# Installing LLVM 20
RUN curl https://apt.llvm.org/llvm-snapshot.gpg.key | tee -a /usr/share/keyrings/llvm-snapshot.gpg.key
RUN echo "deb [signed-by=/usr/share/keyrings/llvm-snapshot.gpg.key] http://apt.llvm.org/trixie/ llvm-toolchain-trixie main" >> /etc/apt/sources.list
RUN echo "deb [signed-by=/usr/share/keyrings/llvm-snapshot.gpg.key] http://apt.llvm.org/trixie/ llvm-toolchain-trixie-20 main" >> /etc/apt/sources.list
RUN echo "deb [signed-by=/usr/share/keyrings/llvm-snapshot.gpg.key] http://apt.llvm.org/trixie/ llvm-toolchain-trixie-21 main" >> /etc/apt/sources.list
RUN apt-get update && apt-get install -y \
    clang-20 lld-20
RUN ln -s /usr/bin/clang-20 /usr/bin/clang
RUN ln -s /usr/bin/clang++-20 /usr/bin/clang++
RUN ln -s /usr/bin/lld-20 /usr/bin/lld

# Adding Source Code
ADD include /app/include
ADD src /app/src
ADD tests /app/tests
ADD CMakeLists.txt /app/CMakeLists.txt
ADD scripts /app/scripts

# Building clice
WORKDIR /app
RUN ls -la scripts
RUN cp scripts/build.sh scripts/build-release.sh && sed -i 's/DCMAKE_BUILD_TYPE=Debug/DCMAKE_BUILD_TYPE=Release/g' scripts/build-release.sh
RUN bash -c ./scripts/build-release.sh

RUN cmake --install build --prefix=/opt/clice

FROM debian:13 

COPY --from=builder /opt/clice /opt/clice/
ADD LICENSE /opt/clice/LICENSE
ADD README.md /opt/clice/README.md

RUN ln -s /opt/clice/bin/clice /usr/local/bin/clice

ENTRYPOINT ["clice"]
