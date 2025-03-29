# Use Ubuntu 24.04 as the base image
FROM ubuntu:24.04
ARG USER=devcontainer

# Update package lists and install essential tools
RUN apt update && \
    apt install -y software-properties-common && \
    add-apt-repository ppa:ubuntu-toolchain-r/test && \
    apt update && \
    apt install -y gcc-14 g++-14 libstdc++-14-dev && \
    apt install -y cmake ninja-build python3 python3-pip git tmux vim curl wget sudo

# Install LLVM 20
RUN wget https://apt.llvm.org/llvm.sh && \
    chmod +x llvm.sh && \
    ./llvm.sh 20 all

RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 100 && \
    update-alternatives --set gcc /usr/bin/gcc-14 && \
    update-alternatives --set g++ /usr/bin/g++-14

# Create a non-root user, set shell to bash, and add to sudo group
RUN useradd -m ${USER} -s /bin/bash && \
    echo "${USER} ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/${USER}

RUN ln -s /usr/bin/lld-20 /usr/bin/lld && \
    ln -s /usr/bin/clang-20 /usr/bin/clang && \
    ln -s /usr/bin/clang++-20 /usr/bin/clang++ && \
    ln -s /usr/bin/clangd-20 /usr/bin/clangd

# Set the default command
CMD ["/bin/bash"]
