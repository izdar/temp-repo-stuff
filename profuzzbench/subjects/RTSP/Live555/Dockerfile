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

# --------------------------------------------------------------------
# 3) Environment Variables
# --------------------------------------------------------------------
ENV WORKDIR="/home/ubuntu/experiments"
ENV AFLNET="/home/ubuntu/aflnet"
ENV PATH="${PATH}:${AFLNET}:/home/ubuntu/.local/bin:${WORKDIR}"
ENV AFL_PATH="${AFLNET}"
ENV AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
    AFL_SKIP_CPUFREQ=1 \
    AFL_NO_AFFINITY=1
ENV MONITOR_VERBOSE=1

# Set up environment variables for ASAN
ENV ASAN_OPTIONS='abort_on_error=1:symbolize=0:detect_leaks=0:detect_stack_use_after_return=1:detect_container_overflow=0:poison_array_cookie=0:malloc_fill_byte=0:max_malloc_fill_size=16777216'

RUN mkdir -p $WORKDIR

# Copy patches and LTL spec
COPY --chown=ubuntu:ubuntu fuzzing.patch ${WORKDIR}/fuzzing.patch
COPY --chown=ubuntu:ubuntu gcov.patch ${WORKDIR}/gcov.patch

# --------------------------------------------------------------------
# 4) Download and compile Live555 for fuzzing
# --------------------------------------------------------------------
RUN cd $WORKDIR && \
    git clone https://github.com/rgaufman/live555.git && \
    cd live555 && \
    git checkout ceeb4f4 && \
    patch -p1 < $WORKDIR/fuzzing.patch && \
    ./genMakefiles linux && \
    AFL_USE_ASAN=1 make clean all $MAKE_OPT || make all

# Set up Live555 for fuzzing
RUN cd ${WORKDIR}/live555/testProgs && \
    cp ${AFLNET}/tutorials/live555/sample_media_sources/*.* ./

# --------------------------------------------------------------------
# 5) Download and compile Live555 for coverage analysis
# --------------------------------------------------------------------
RUN cd $WORKDIR && \
    git clone https://github.com/rgaufman/live555.git live555-cov && \
    cd live555-cov && \
    git checkout ceeb4f4 && \
    patch -p1 < $WORKDIR/gcov.patch && \
    ./genMakefiles linux && \
    make CFLAGS="-fprofile-arcs -ftest-coverage" CPPFLAGS="-fprofile-arcs -ftest-coverage" CXXFLAGS="-fprofile-arcs -ftest-coverage" LDFLAGS="-fprofile-arcs -ftest-coverage" clean all

# Set up Live555 for coverage analysis
RUN cd ${WORKDIR}/live555-cov/testProgs && \
    cp ${AFLNET}/tutorials/live555/sample_media_sources/*.* ./

# --------------------------------------------------------------------
# 6) Copy fuzzing scripts and inputs
# --------------------------------------------------------------------
COPY --chown=ubuntu:ubuntu kill-server.sh ${WORKDIR}/kill-server
COPY --chown=ubuntu:ubuntu in-rtsp ${WORKDIR}/in-rtsp
COPY --chown=ubuntu:ubuntu rtsp.dict ${WORKDIR}/rtsp.dict
COPY --chown=ubuntu:ubuntu cov_script.sh ${WORKDIR}/cov_script
COPY --chown=ubuntu:ubuntu run.sh ${WORKDIR}/run

# Make scripts executable
RUN chmod +x ${WORKDIR}/kill-server ${WORKDIR}/cov_script ${WORKDIR}/run
