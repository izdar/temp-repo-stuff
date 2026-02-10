FROM ubuntu:20.04

# Install common dependencies
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
    libfl-dev \
    pkg-config && \
    rm -rf /var/lib/apt/lists/*

# Add a new user ubuntu, pass: ubuntu
RUN groupadd ubuntu && \
    useradd -rm -d /home/ubuntu -s /bin/bash -g ubuntu -G sudo -u 1000 ubuntu -p "$(openssl passwd -1 ubuntu)"

RUN chmod 777 /tmp

RUN pip3 install gcovr==4.2

# Use ubuntu as default username
USER ubuntu
WORKDIR /home/ubuntu

# Import environment variable to pass as parameter to make (e.g., to make parallel builds with -j)
ARG MAKE_OPT

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
COPY --chown=ubuntu:ubuntu monitor-src/monitor_bridge.* /home/ubuntu/aflnet/
COPY --chown=ubuntu:ubuntu monitor-src/ssh_predicate_adapter.* /home/ubuntu/aflnet/
COPY --chown=ubuntu:ubuntu monitor-src/rtsp_predicate_adapter.* /home/ubuntu/aflnet/
COPY --chown=ubuntu:ubuntu monitor-src/sip_predicate_adapter.* /home/ubuntu/aflnet/


# IMPORTANT: Copy your modified Makefile, aflnet.c, and afl-fuzz.c 
# This overwrites the git-cloned versions with your monitored versions
COPY --chown=ubuntu:ubuntu Makefile /home/ubuntu/aflnet/Makefile
COPY --chown=ubuntu:ubuntu aflnet.c /home/ubuntu/aflnet/aflnet.c
COPY --chown=ubuntu:ubuntu afl-fuzz.c /home/ubuntu/aflnet/afl-fuzz.c
COPY --chown=ubuntu:ubuntu aflnet.h /home/ubuntu/aflnet/aflnet.h
COPY --chown=ubuntu:ubuntu config.h /home/ubuntu/aflnet/config.h

# Set up environment variables for AFLNet
ENV WORKDIR="/home/ubuntu/experiments"
ENV AFLNET="/home/ubuntu/aflnet"
ENV PATH="${PATH}:${AFLNET}:/home/ubuntu/.local/bin:${WORKDIR}"
ENV AFL_PATH="${AFLNET}"
ENV AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
    AFL_SKIP_CPUFREQ=1 \
    AFL_NO_AFFINITY=1

# BUILD AFLNET BEFORE USING IT
RUN cd ${AFLNET} && make clean all -j$(nproc)

RUN mkdir $WORKDIR

COPY --chown=ubuntu:ubuntu kamailio.patch ${WORKDIR}/kamailio.patch
COPY --chown=ubuntu:ubuntu pjsip.patch ${WORKDIR}/pjsip.patch

# Set up environment variables for ASAN
ENV ASAN_OPTIONS='abort_on_error=1:symbolize=0:detect_leaks=0:detect_stack_use_after_return=1:detect_container_overflow=0:poison_array_cookie=0:malloc_fill_byte=0:max_malloc_fill_size=16777216'

# Download and compile Kamailio for fuzzing
RUN cd $WORKDIR && \
    git clone https://github.com/kamailio/kamailio.git && \
    cd kamailio && \
    git checkout 2648eb3 && \
    patch -p1 < $WORKDIR/kamailio.patch && \
    export CC="$AFLNET/afl-gcc" && \
    export CXX="$AFLNET/afl-g++" && \
    export LD="$AFLNET/afl-gcc" && \
    make MEMPKG=sys cfg && \
    make all $MAKE_OPT

# Build SIP client to simulate the callee
RUN cd $WORKDIR && \
    git clone https://github.com/pjsip/pjproject.git && \
    cd pjproject && \
    git checkout bba95b8 && \
    patch -p1 < $WORKDIR/pjsip.patch && \
    ./configure && \
    make dep && make clean && make $MAKE_OPT

# Download and compile Kamalio for code coverage analysis
RUN cd $WORKDIR && \
    git clone https://github.com/kamailio/kamailio.git kamailio-gcov && \
    cd kamailio-gcov && \
    git checkout 2648eb3 && \
    patch -p1 < $WORKDIR/kamailio.patch && \
    make CC_EXTRA_OPTS="-fprofile-arcs -ftest-coverage" LD_EXTRA_OPTS="-fprofile-arcs -ftest-coverage" MEMPKG=sys cfg && \
    make CC_EXTRA_OPTS="-fprofile-arcs -ftest-coverage" LD_EXTRA_OPTS="-fprofile-arcs -ftest-coverage" all $MAKE_OPT

COPY --chown=ubuntu:ubuntu in-sip ${WORKDIR}/in-sip
COPY --chown=ubuntu:ubuntu cov_script.sh ${WORKDIR}/cov_script
COPY --chown=ubuntu:ubuntu run.sh ${WORKDIR}/run
COPY --chown=ubuntu:ubuntu run_pjsip.sh ${WORKDIR}/run_pjsip
COPY --chown=ubuntu:ubuntu kamailio-basic.cfg ${WORKDIR}/kamailio-basic.cfg
COPY --chown=ubuntu:ubuntu StarWars3.wav ${WORKDIR}/StarWars3.wav
COPY --chown=ubuntu:ubuntu monitor-bin/kamailio.txt /home/ubuntu/monitor/kamailio.txt