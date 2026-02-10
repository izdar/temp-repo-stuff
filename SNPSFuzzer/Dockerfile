# SNPSFuzzer build + runtime container (Ubuntu 18.04 + LLVM/Clang 6)
# - Builds criu4snpsfuzzer
# - Builds SNPSFuzzer (AFLNet fork)
# - Builds llvm_mode with clang-6.0/llvm-config-6.0
#
# NOTE: Snapshotting (CRIU) typically needs extra privileges at runtime.
# See the "How to run" section at the bottom of this file.

FROM ubuntu:18.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

# Core build deps + SNPSFuzzer deps from README + older LLVM/Clang toolchain.
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    git \
    build-essential \
    make \
    pkg-config \
    python \
    wget \
    curl \
    vim \
    gdb \
    strace \
    # criu4snpsfuzzer deps
    libprotobuf-dev \
    libprotobuf-c-dev \
    protobuf-c-compiler \
    protobuf-compiler \
    python-protobuf \
    libnl-3-dev \
    libnet-dev \
    libbsd-dev \
    libaio-dev \
    libcap-dev \
    asciidoc \
    xmlto \
    # AFL/AFLNet deps
    graphviz-dev \
    # LLVM/Clang (pick a version that matches older AFL llvm_mode expectations)
    llvm-6.0 \
    llvm-6.0-dev \
    clang-6.0 \
    && rm -rf /var/lib/apt/lists/*

# Make clang/llvm-config default inside the container (optional but convenient)
RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-6.0 100 && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-6.0 100 && \
    update-alternatives --install /usr/bin/llvm-config llvm-config /usr/bin/llvm-config-6.0 100

# Workdir where we'll clone and build
WORKDIR /opt

# Clone SNPSFuzzer
RUN git clone https://github.com/SNPSFuzzer/SNPSFuzzer.git
ENV SNPSFUZZER=/opt/SNPSFuzzer

# Build criu4snpsfuzzer (as README Step II.1)
WORKDIR /opt/SNPSFuzzer/criu4snpsfuzzer
RUN make -j"$(nproc)" && \
    make install && \
    cp ./lib/c/libcriu.so /usr/local/lib/libcriu.so.2 && \
    ldconfig

# Build SNPSFuzzer (as README Step III, first "make")
WORKDIR /opt/SNPSFuzzer
RUN make -j"$(nproc)"

# Build llvm_mode with LLVM/Clang 6 (as README Step III, "cd llvm_mode; make")
WORKDIR /opt/SNPSFuzzer/llvm_mode
RUN make clean || true && \
    make -j"$(nproc)" LLVM_CONFIG=llvm-config-6.0 CC=clang-6.0 CXX=clang++-6.0

# Install SNPSFuzzer (as README Step III, "sudo make install")
WORKDIR /opt/SNPSFuzzer
RUN make install

# Convenience: default workdir on container start
WORKDIR /work

# ------------------------------------------------------------------------------
# HOW TO BUILD:
#   docker build -t snpsfuzzer:18.04-llvm6 .
#
# HOW TO RUN (IMPORTANT for CRIU snapshots):
#   You usually need elevated privileges (CRIU uses namespaces, ptrace, etc).
#   Start with:
#     docker run --rm -it \
#       --privileged \
#       --cap-add=SYS_PTRACE --cap-add=SYS_ADMIN \
#       --security-opt seccomp=unconfined \
#       --security-opt apparmor=unconfined \
#       -v "$PWD":/work \
#       snpsfuzzer:18.04-llvm6 bash
#
# Inside the container, SNPSFuzzer is installed and the repo is at:
#   /opt/SNPSFuzzer
#
# If you want to fuzz OpenSSH, you'll typically:
#  - build OpenSSH in /work with afl-clang-fast (from /opt/SNPSFuzzer or installed path)
#  - run afl-fuzz (SNPSFuzzer’s afl-fuzz) with your AFLNet-style harness
# ------------------------------------------------------------------------------
