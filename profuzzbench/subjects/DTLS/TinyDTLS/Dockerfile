FROM ubuntu:20.04

# Install common dependencies + specific dev libs for Graphviz and Capabilities
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get -y update && \
    apt-get -y install sudo \ 
    apt-utils \
    build-essential \
    openssl \
    clang \
    graphviz-dev \
    libcap-dev \
    git \
    autoconf \
    libgnutls28-dev \
    libssl-dev \
    llvm \
    python3-pip \
    nano \
    net-tools \
    vim \
    gdb \
    netcat \
    strace \
    wget \
    bison \
    flex \
    cmake \
    libfl-dev && \
    rm -rf /var/lib/apt/lists/*

# Add user ubuntu
RUN groupadd ubuntu && \
    useradd -rm -d /home/ubuntu -s /bin/bash -g ubuntu -G sudo -u 1000 ubuntu -p "$(openssl passwd -1 ubuntu)"
RUN chmod 777 /tmp
RUN pip3 install gcovr==4.2

USER ubuntu
WORKDIR /home/ubuntu
ARG MAKE_OPT

# --------------------------------------------------------------------
# 1) Build Custom LTL Evaluator (Multi-protocol: SSH + RTSP)
# --------------------------------------------------------------------
COPY --chown=ubuntu:ubuntu evaluator-src/ /home/ubuntu/evaluator-src/

RUN cd /home/ubuntu/evaluator-src && \
    sed -i 's/-std=c++20/-std=c++2a/g' Makefile || true && \
    make clean && make -j$(nproc)

# Switch to root to copy the binary to /usr/local/bin
USER root
RUN cp /home/ubuntu/evaluator-src/formula_parser /usr/local/bin/formula_parser && \
    chmod +x /usr/local/bin/formula_parser

# Switch back to ubuntu for the rest of the build
USER ubuntu

# --------------------------------------------------------------------
# 2) Set up Modified AFLNet with RTSP Support
# --------------------------------------------------------------------
RUN git clone https://github.com/profuzzbench/aflnet.git && \
    mkdir -p /home/ubuntu/monitor

# Copy bridge and protocol adapters (SSH + RTSP)
COPY --chown=ubuntu:ubuntu monitor-src/ /home/ubuntu/aflnet/
COPY --chown=ubuntu:ubuntu monitor-bin/ /home/ubuntu/monitor/
# COPY --chown=ubuntu:ubuntu monitor-src/ssh_predicate_adapter.* /home/ubuntu/aflnet/
# COPY --chown=ubuntu:ubuntu monitor-src/rtsp_predicate_adapter.* /home/ubuntu/aflnet/

# IMPORTANT: Copy your modified Makefile, aflnet.c, and afl-fuzz.c 
# This overwrites the git-cloned versions with your monitored versions
COPY --chown=ubuntu:ubuntu Makefile /home/ubuntu/aflnet/Makefile
COPY --chown=ubuntu:ubuntu aflnet.c /home/ubuntu/aflnet/aflnet.c
COPY --chown=ubuntu:ubuntu afl-fuzz.c /home/ubuntu/aflnet/afl-fuzz.c
COPY --chown=ubuntu:ubuntu aflnet.h /home/ubuntu/aflnet/aflnet.h
COPY --chown=ubuntu:ubuntu config.h /home/ubuntu/aflnet/config.h

# Build AFLNet (Makefile now includes -lcap -lgvc -lcgraph and rtsp_predicate_adapter.o)
RUN cd /home/ubuntu/aflnet && \
    make clean all $MAKE_OPT && \
    cd llvm_mode && make $MAKE_OPT

# Set up environment variables for AFLNet
ENV WORKDIR="/home/ubuntu/experiments"
ENV AFLNET="/home/ubuntu/aflnet"
ENV PATH="${PATH}:${AFLNET}:/home/ubuntu/.local/bin:${WORKDIR}"
ENV AFL_PATH="${AFLNET}"
ENV AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
    AFL_SKIP_CPUFREQ=1 \
    AFL_NO_AFFINITY=1


RUN mkdir $WORKDIR

COPY --chown=ubuntu:ubuntu gcov.patch ${WORKDIR}/gcov.patch

# Set up environment variables for ASAN
env ASAN_OPTIONS='abort_on_error=1:symbolize=0:detect_leaks=0:detect_stack_use_after_return=1:detect_container_overflow=0:poison_array_cookie=0:malloc_fill_byte=0:max_malloc_fill_size=16777216'

# Download and compile TinyDTLS for fuzzing
RUN cd $WORKDIR && \
    git clone https://github.com/assist-project/tinydtls-fuzz.git tinydtls && \
    cd tinydtls && \
    git checkout 06995d4 && \
    cd tests && \
    CC=afl-clang-fast make clean && \
    AFL_USE_ASAN=1 CC=afl-clang-fast make ../libtinydtls.a $MAKE_OPT && \
    AFL_USE_ASAN=1 CC=afl-clang-fast make $MAKE_OPT

# Download and compile TinyDTLS for coverage analysis
RUN cd $WORKDIR && \
    git clone https://github.com/assist-project/tinydtls-fuzz.git tinydtls-gcov && \
    cd tinydtls-gcov && \
    git checkout 06995d4 && \
    patch -p1 < $WORKDIR/gcov.patch && \
    cd tests && \
    cov=1 make clean && \
    cov=1 make ../libtinydtls.a $MAKE_OPT && \
    cov=1 make $MAKE_OPT

COPY --chown=ubuntu:ubuntu in-dtls ${WORKDIR}/in-dtls
COPY --chown=ubuntu:ubuntu cov_script.sh ${WORKDIR}/cov_script
COPY --chown=ubuntu:ubuntu run.sh ${WORKDIR}/run
