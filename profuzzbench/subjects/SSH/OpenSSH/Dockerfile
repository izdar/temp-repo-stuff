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
# 1) Build Custom LTL Evaluator
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
# 2) Set up Modified AFLNet
# --------------------------------------------------------------------
RUN git clone https://github.com/profuzzbench/aflnet.git && \
    mkdir -p /home/ubuntu/monitor

# Copy your bridge/adapter sources and modified core files
# COPY --chown=ubuntu:ubuntu monitor-src/monitor_bridge.* /home/ubuntu/aflnet/
COPY --chown=ubuntu:ubuntu monitor-src/ /home/ubuntu/aflnet/

# IMPORTANT: Copy your modified Makefile, aflnet.c, and afl-fuzz.c 
# This overwrites the git-cloned versions with your monitored versions
COPY --chown=ubuntu:ubuntu Makefile /home/ubuntu/aflnet/Makefile
COPY --chown=ubuntu:ubuntu afl-fuzz.c /home/ubuntu/aflnet/afl-fuzz.c
COPY --chown=ubuntu:ubuntu aflnet.h /home/ubuntu/aflnet/aflnet.h
COPY --chown=ubuntu:ubuntu aflnet.c /home/ubuntu/aflnet/aflnet.c
COPY --chown=ubuntu:ubuntu config.h /home/ubuntu/aflnet/config.h

# Build AFLNet (Makefile now includes -lcap -lgvc -lcgraph)
RUN cd /home/ubuntu/aflnet && \
    make clean all $MAKE_OPT && \
    cd llvm_mode && make $MAKE_OPT

# --------------------------------------------------------------------
# 3) Environment & Dependencies for OpenSSH
# --------------------------------------------------------------------
ENV WORKDIR="/home/ubuntu/experiments"
ENV AFLNET="/home/ubuntu/aflnet"
ENV PATH="${PATH}:${AFLNET}:/home/ubuntu/.local/bin:${WORKDIR}"
ENV AFL_PATH="${AFLNET}"
ENV AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
    AFL_SKIP_CPUFREQ=1 \
    AFL_NO_AFFINITY=1
ENV MONITOR_VERBOSE=1

RUN mkdir -p $WORKDIR
USER root
RUN apt-get update && apt-get -y install sshpass
USER ubuntu

COPY --chown=ubuntu:ubuntu rand.patch ${WORKDIR}/rand.patch
COPY --chown=ubuntu:ubuntu rand.inc ${WORKDIR}/rand.inc
COPY --chown=ubuntu:ubuntu monitor-bin/ssh-refined.txt /home/ubuntu/monitor/ssh-refined.txt

# Download and compile OpenSSL 1.0.2
RUN cd ${WORKDIR} && \
    git clone https://github.com/openssl/openssl openssl && \
    cd openssl && git checkout OpenSSL_1_0_2u && \
    ./Configure linux-x86_64-clang shared --prefix=$WORKDIR/openssl-install && \
    make $MAKE_OPT && make install

ENV LD_LIBRARY_PATH="${WORKDIR}/openssl-install/lib"

# --------------------------------------------------------------------
# 4) Compile Instrumented OpenSSH
# --------------------------------------------------------------------
RUN cd ${WORKDIR} && \
    git clone https://github.com/vegard/openssh-portable.git openssh && \
    cd openssh && git checkout 7cfea58 && \
    cp ${WORKDIR}/rand.inc . && patch -p1 < ${WORKDIR}/rand.patch && \
    autoreconf && \
    ./configure \
    CC="afl-clang-fast" \
    CFLAGS="-g -O3 -I$WORKDIR/openssl-install/include" \
    --prefix=$PWD/install \
    --with-openssl=$WORKDIR/openssl-install \
    --with-ldflags="-L$WORKDIR/openssl-install/lib" \
    --with-privsep-path=$PWD/var-empty \
    --with-sandbox=no \
    --with-privsep-user=ubuntu && \
    make $MAKE_OPT && make install

# Download and compile OpenSSH for coverage analysis
RUN cd ${WORKDIR} && \
    git clone https://github.com/vegard/openssh-portable.git openssh-gcov && \
    cd openssh-gcov && git checkout 7cfea58 && \
    cp ${WORKDIR}/rand.inc . && patch -p1 < ${WORKDIR}/rand.patch && \
    autoreconf && \
    ./configure \
    CC="gcc" \
    CFLAGS="-g -O3 -fprofile-arcs -ftest-coverage -I$WORKDIR/openssl-install/include" \
    LDFLAGS="-fprofile-arcs -ftest-coverage" \
    --with-openssl=$WORKDIR/openssl-install \
    --with-ldflags="-L$WORKDIR/openssl-install/lib" \
    --prefix=$PWD/install \
    --with-privsep-path=$PWD/var-empty \
    --with-sandbox=no \
    --with-privsep-user=ubuntu && \
    make $MAKE_OPT && make install

COPY --chown=ubuntu:ubuntu in-ssh ${WORKDIR}/in-ssh
COPY --chown=ubuntu:ubuntu ssh.dict ${WORKDIR}/ssh.dict
COPY --chown=ubuntu:ubuntu cov_script.sh ${WORKDIR}/cov_script.sh
COPY --chown=ubuntu:ubuntu run.sh ${WORKDIR}/run

# Set up environment variables for ASAN
ENV ASAN_OPTIONS='abort_on_error=1:symbolize=0:detect_leaks=0:detect_stack_use_after_return=1:detect_container_overflow=0:poison_array_cookie=0:malloc_fill_byte=0:max_malloc_fill_size=16777216'
