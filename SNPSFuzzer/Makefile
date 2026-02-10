#
# american fuzzy lop - makefile (SNPSFuzzer with Monitor Integration)
# ---------------------------------------------------------------------
#
# Written and maintained by Michal Zalewski <lcamtuf@google.com>
# Monitor integration by [Your Name]
#
# Copyright 2013, 2014, 2015, 2016, 2017 Google LLC All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#   http://www.apache.org/licenses/LICENSE-2.0
#

PROGNAME    = afl
VERSION     = $(shell grep '^\#define VERSION ' config.h | cut -d '"' -f2)

PREFIX     ?= /usr/local
BIN_PATH    = $(PREFIX)/bin
HELPER_PATH = $(PREFIX)/lib/afl
DOC_PATH    = $(PREFIX)/share/doc/afl
MISC_PATH   = $(PREFIX)/share/afl

# PROGS intentionally omit afl-as, which gets installed elsewhere.
PROGS       = hook_socket.so afl-gcc afl-fuzz afl-replay aflnet-replay afl-showmap afl-tmin afl-gotcpu afl-analyze formula_parser

SH_PROGS    = afl-plot afl-cmin afl-whatsup

# Build flags
# CFLAGS     ?= -O0 -funroll-loops -DSNAPSHOT_DEBUG -DDEBUG
CFLAGS     ?= -O0 -funroll-loops
CFLAGS     += -Wall -D_FORTIFY_SOURCE=2 -g -Wno-pointer-sign -Wno-unused-result \
	      -DAFL_PATH=\"$(HELPER_PATH)\" -DDOC_PATH=\"$(DOC_PATH)\" \
	      -DBIN_PATH=\"$(BIN_PATH)\"

# C++ flags for evaluator (matching evaluator-src Makefile)
CXX        ?= g++
CXXFLAGS   ?= -Wall -g -std=c++20

# Check OS and set appropriate flex library
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
 FLEXLIB =
else
 FLEXLIB = -lfl
endif

# Include directories
INC_DIRS := -I ./criu4snpsfuzzer/lib/c/ -I ./criu4snpsfuzzer/soccr/ -I ./monitor-src/ -I ./evaluator-src/
LIB_DIRS := -L ./criu4snpsfuzzer/lib/c/ -lcriu -L ./criu4snpsfuzzer/soccr/ -lsoccr

ifneq "$(filter Linux GNU%,$(shell uname))" ""
  LDFLAGS  += -ldl -lgvc -lcgraph -lm -lrt -Werror
endif

ifeq "$(findstring clang, $(shell $(CC) --version 2>/dev/null))" ""
  TEST_CC   = afl-gcc
else
  TEST_CC   = afl-clang
endif

COMM_HDR    = alloc-inl.h config.h debug.h types.h

# --- Snapshot log object ---
SNAPSHOT_LOG_OBJ = snapshot_log.o

# --- Monitor bridge and predicate adapter objects ---
MONITOR_OBJS = monitor-src/monitor_bridge.o \
               monitor-src/ssh_predicate_adapter.o \
               monitor-src/rtsp_predicate_adapter.o \
               monitor-src/ftp_predicate_adapter.o \
			   monitor-src/dtls_predicate_adapter.o \
			   monitor-src/sip_predicate_adapter.o \
			   monitor-src/dnsmasq_predicate_adapter.o

# --- Evaluator objects (C++) ---
# Note: lexer.cpp and parser.cpp are generated from .l and .y files
EVALUATOR_OBJS = evaluator-src/parser.o \
                 evaluator-src/lexer.o \
                 evaluator-src/ast_printer.o \
                 evaluator-src/memory_manager.o \
                 evaluator-src/main.o \
                 evaluator-src/typechecker.o \
                 evaluator-src/preprocess.o \
                 evaluator-src/state.o \
                 evaluator-src/evaluator.o \
                 evaluator-src/bitvector.o

# Common objects linked into most tools
COMMON_OBJS = $(SNAPSHOT_LOG_OBJ)

all: test_x86 $(PROGS) afl-as test_build all_done

ifndef AFL_NO_X86

test_x86:
	@echo "[*] Checking for the ability to compile x86 code..."
	@echo 'main() { __asm__("xorb %al, %al"); }' | $(CC) -w -x c - -o .test || ( echo; echo "Oops, looks like your compiler can't generate x86 code."; echo; echo "Don't panic! You can use the LLVM or QEMU mode, but see docs/INSTALL first."; echo "(To ignore this error, set AFL_NO_X86=1 and try again.)"; echo; exit 1 )
	@rm -f .test
	@echo "[+] Everything seems to be working, ready to compile."

else

test_x86:
	@echo "[!] Note: skipping x86 compilation checks (AFL_NO_X86 set)."

endif

# --- Build rule for snapshot_log.o ---
snapshot_log.o: snapshot_log.c
	$(CC) $(CFLAGS) -c -o $@ $<

# --- Build rules for monitor bridge and predicate adapters ---
monitor-src/monitor_bridge.o: monitor-src/monitor_bridge.c monitor-src/monitor_bridge.h
	$(CC) $(CFLAGS) -c -o $@ monitor-src/monitor_bridge.c

monitor-src/ssh_predicate_adapter.o: monitor-src/ssh_predicate_adapter.c monitor-src/ssh_predicate_adapter.h
	$(CC) $(CFLAGS) -c -o $@ monitor-src/ssh_predicate_adapter.c

monitor-src/rtsp_predicate_adapter.o: monitor-src/rtsp_predicate_adapter.c monitor-src/rtsp_predicate_adapter.h
	$(CC) $(CFLAGS) -c -o $@ monitor-src/rtsp_predicate_adapter.c

monitor-src/ftp_predicate_adapter.o: monitor-src/ftp_predicate_adapter.c monitor-src/ftp_predicate_adapter.h
	$(CC) $(CFLAGS) -c -o $@ monitor-src/ftp_predicate_adapter.c

monitor-src/dtls_predicate_adapter.o: monitor-src/dtls_predicate_adapter.c monitor-src/dtls_predicate_adapter.h
	$(CC) $(CFLAGS) -c -o $@ monitor-src/dtls_predicate_adapter.c

monitor-src/sip_predicate_adapter.o: monitor-src/sip_predicate_adapter.c monitor-src/sip_predicate_adapter.h
	$(CC) $(CFLAGS) -c -o $@ monitor-src/sip_predicate_adapter.c

monitor-src/dnsmasq_predicate_adapter.o: monitor-src/dnsmasq_predicate_adapter.c monitor-src/dnsmasq_predicate_adapter.h
	$(CC) $(CFLAGS) -c -o $@ monitor-src/dnsmasq_predicate_adapter.c
	
# --- Build rules for evaluator (C++) ---

# Generate lexer.cpp from lexer.l using flex
evaluator-src/lexer.cpp: evaluator-src/lexer.l
	flex -o evaluator-src/lexer.cpp evaluator-src/lexer.l

# Generate parser.cpp and parser.hpp from parser.y using bison
evaluator-src/parser.cpp evaluator-src/parser.hpp: evaluator-src/parser.y
	bison -d -o evaluator-src/parser.cpp evaluator-src/parser.y

# Compile generated lexer
evaluator-src/lexer.o: evaluator-src/lexer.cpp
	$(CXX) $(CXXFLAGS) -I./evaluator-src -c -o $@ evaluator-src/lexer.cpp

# Compile generated parser
evaluator-src/parser.o: evaluator-src/parser.cpp evaluator-src/parser.hpp
	$(CXX) $(CXXFLAGS) -I./evaluator-src -c -o $@ evaluator-src/parser.cpp

# Compile other evaluator objects
evaluator-src/ast_printer.o: evaluator-src/ast_printer.cpp evaluator-src/ast_printer.h
	$(CXX) $(CXXFLAGS) -I./evaluator-src -c -o $@ evaluator-src/ast_printer.cpp

evaluator-src/memory_manager.o: evaluator-src/memory_manager.cpp evaluator-src/memory_manager.h
	$(CXX) $(CXXFLAGS) -I./evaluator-src -c -o $@ evaluator-src/memory_manager.cpp

evaluator-src/typechecker.o: evaluator-src/typechecker.cpp evaluator-src/typechecker.h
	$(CXX) $(CXXFLAGS) -I./evaluator-src -c -o $@ evaluator-src/typechecker.cpp

evaluator-src/preprocess.o: evaluator-src/preprocess.cpp evaluator-src/preprocess.h
	$(CXX) $(CXXFLAGS) -I./evaluator-src -c -o $@ evaluator-src/preprocess.cpp

evaluator-src/state.o: evaluator-src/state.cpp evaluator-src/state.h
	$(CXX) $(CXXFLAGS) -I./evaluator-src -c -o $@ evaluator-src/state.cpp

evaluator-src/evaluator.o: evaluator-src/evaluator.cpp evaluator-src/evaluator.h
	$(CXX) $(CXXFLAGS) -I./evaluator-src -c -o $@ evaluator-src/evaluator.cpp

evaluator-src/bitvector.o: evaluator-src/bitvector.cpp evaluator-src/bitvector.h
	$(CXX) $(CXXFLAGS) -I./evaluator-src -c -o $@ evaluator-src/bitvector.cpp

evaluator-src/main.o: evaluator-src/main.cpp
	$(CXX) $(CXXFLAGS) -I./evaluator-src -c -o $@ evaluator-src/main.cpp

# --- LTL Formula Parser (Evaluator executable) ---
formula_parser: $(EVALUATOR_OBJS)
	$(CXX) $(CXXFLAGS) $(EVALUATOR_OBJS) -o $@ $(FLEXLIB)

# --- aflnet.o now includes monitor objects ---
aflnet.o: aflnet.c aflnet.h $(MONITOR_OBJS)
	$(CC) $(CFLAGS) $(INC_DIRS) -c aflnet.c -o aflnet.o

afl-gcc: afl-gcc.c $(COMM_HDR) | test_x86
	$(CC) $(CFLAGS) $@.c -o $@ $(LDFLAGS)
	set -e; for i in afl-g++ afl-clang afl-clang++; do ln -sf afl-gcc $$i; done

afl-as: afl-as.c afl-as.h $(COMM_HDR) $(COMMON_OBJS) | test_x86
	$(CC) $(CFLAGS) $@.c $(COMMON_OBJS) -o $@ $(LDFLAGS)
	ln -sf afl-as as

hook_socket.so: hook_socket.c
	$(CC) $(CFLAGS) -fPIC -shared -o hook_socket.so hook_socket.c -ldl

afl-fuzz: afl-fuzz.c $(COMM_HDR) aflnet.o aflnet.h $(MONITOR_OBJS) $(COMMON_OBJS) | test_x86
	$(CC) $(CFLAGS) $(INC_DIRS) $@.c aflnet.o $(MONITOR_OBJS) $(COMMON_OBJS) ./criu4snpsfuzzer/lib/c/criu.o $(LIB_DIRS) -o $@ $(LDFLAGS)

afl-replay: afl-replay.c $(COMM_HDR) aflnet.o aflnet.h $(MONITOR_OBJS) $(COMMON_OBJS) | test_x86
	$(CC) $(CFLAGS) $(INC_DIRS) $@.c aflnet.o $(MONITOR_OBJS) $(COMMON_OBJS) -o $@ $(LDFLAGS)

aflnet-replay: aflnet-replay.c $(COMM_HDR) aflnet.o aflnet.h $(MONITOR_OBJS) $(COMMON_OBJS) | test_x86
	$(CC) $(CFLAGS) $(INC_DIRS) $@.c aflnet.o $(MONITOR_OBJS) $(COMMON_OBJS) -o $@ $(LDFLAGS)

afl-showmap: afl-showmap.c $(COMM_HDR) $(COMMON_OBJS) | test_x86
	$(CC) $(CFLAGS) $@.c $(COMMON_OBJS) -o $@ $(LDFLAGS)

afl-tmin: afl-tmin.c $(COMM_HDR) $(COMMON_OBJS) | test_x86
	$(CC) $(CFLAGS) $@.c $(COMMON_OBJS) -o $@ $(LDFLAGS)

afl-analyze: afl-analyze.c $(COMM_HDR) $(COMMON_OBJS) | test_x86
	$(CC) $(CFLAGS) $@.c $(COMMON_OBJS) -o $@ $(LDFLAGS)

afl-gotcpu: afl-gotcpu.c $(COMM_HDR) $(COMMON_OBJS) | test_x86
	$(CC) $(CFLAGS) $@.c $(COMMON_OBJS) -o $@ $(LDFLAGS)

ifndef AFL_NO_X86

test_build: afl-gcc afl-as afl-showmap
	@echo "[*] Testing the CC wrapper and instrumentation output..."
	unset AFL_USE_ASAN AFL_USE_MSAN; AFL_QUIET=1 AFL_INST_RATIO=100 AFL_PATH=. ./$(TEST_CC) $(CFLAGS) test-instr.c -o test-instr $(LDFLAGS)
	./afl-showmap -m none -q -o .test-instr0 ./test-instr < /dev/null
	echo 1 | ./afl-showmap -m none -q -o .test-instr1 ./test-instr
	@rm -f test-instr
	@cmp -s .test-instr0 .test-instr1; DR="$$?"; rm -f .test-instr0 .test-instr1; if [ "$$DR" = "0" ]; then echo; echo "Oops, the instrumentation does not seem to be behaving correctly!"; echo; echo "Please ping <lcamtuf@google.com> to troubleshoot the issue."; echo; exit 1; fi
	@echo "[+] All right, the instrumentation seems to be working!"

else

test_build: afl-gcc afl-as afl-showmap
	@echo "[!] Note: skipping build tests (you may need to use LLVM or QEMU mode)."

endif

all_done: test_build
	@if [ ! "`which clang 2>/dev/null`" = "" ]; then echo "[+] LLVM users: see llvm_mode/README.llvm for a faster alternative to afl-gcc."; fi
	@echo "[+] All done! Be sure to review README - it's pretty short and useful."
	@echo "[+] LTL formula parser built: formula_parser (with monitor integration)"
	@if [ "`uname`" = "Darwin" ]; then printf "\nWARNING: Fuzzing on MacOS X is slow because of the unusually high overhead of\nfork() on this OS. Consider using Linux or *BSD. You can also use VirtualBox\n(virtualbox.org) to put AFL inside a Linux or *BSD VM.\n\n"; fi
	@! tty <&1 >/dev/null || printf "\033[0;30mNOTE: If you can read this, your terminal probably uses white background.\nThis will make the UI hard to read. See docs/status_screen.txt for advice.\033[0m\n" 2>/dev/null

.NOTPARALLEL: clean

clean:
	rm -f $(PROGS) afl-as as afl-g++ afl-clang afl-clang++ *.o *~ a.out core core.[1-9][0-9]* *.stackdump test .test test-instr .test-instr0 .test-instr1 qemu_mode/qemu-2.10.0.tar.bz2 afl-qemu-trace
	rm -f monitor-src/*.o
	rm -f evaluator-src/*.o evaluator-src/lexer.cpp evaluator-src/parser.cpp evaluator-src/parser.hpp
	rm -f formula_parser
	rm -rf out_dir qemu_mode/qemu-2.10.0
	$(MAKE) -C llvm_mode clean
	$(MAKE) -C libdislocator clean
	$(MAKE) -C libtokencap clean

install: all
	mkdir -p -m 755 $${DESTDIR}$(BIN_PATH) $${DESTDIR}$(HELPER_PATH) $${DESTDIR}$(DOC_PATH) $${DESTDIR}$(MISC_PATH)
	rm -f $${DESTDIR}$(BIN_PATH)/afl-plot.sh
	install -m 755 $(PROGS) $(SH_PROGS) $${DESTDIR}$(BIN_PATH)
	rm -f $${DESTDIR}$(BIN_PATH)/afl-as
	if [ -f afl-qemu-trace ]; then install -m 755 afl-qemu-trace $${DESTDIR}$(BIN_PATH); fi
ifndef AFL_TRACE_PC
	if [ -f afl-clang-fast -a -f afl-llvm-pass.so -a -f afl-llvm-rt.o ]; then set -e; install -m 755 afl-clang-fast $${DESTDIR}$(BIN_PATH); ln -sf afl-clang-fast $${DESTDIR}$(BIN_PATH)/afl-clang-fast++; install -m 755 afl-llvm-pass.so afl-llvm-rt.o $${DESTDIR}$(HELPER_PATH); fi
else
	if [ -f afl-clang-fast -a -f afl-llvm-rt.o ]; then set -e; install -m 755 afl-clang-fast $${DESTDIR}$(BIN_PATH); ln -sf afl-clang-fast $${DESTDIR}$(BIN_PATH)/afl-clang-fast++; install -m 755 afl-llvm-rt.o $${DESTDIR}$(HELPER_PATH); fi
endif
	if [ -f afl-llvm-rt-32.o ]; then set -e; install -m 755 afl-llvm-rt-32.o $${DESTDIR}$(HELPER_PATH); fi
	if [ -f afl-llvm-rt-64.o ]; then set -e; install -m 755 afl-llvm-rt-64.o $${DESTDIR}$(HELPER_PATH); fi
	set -e; for i in afl-g++ afl-clang afl-clang++; do ln -sf afl-gcc $${DESTDIR}$(BIN_PATH)/$$i; done
	install -m 755 afl-as $${DESTDIR}$(HELPER_PATH)
	ln -sf afl-as $${DESTDIR}$(HELPER_PATH)/as
	install -m 644 docs/README docs/ChangeLog docs/*.txt $${DESTDIR}$(DOC_PATH)
	cp -r testcases/ $${DESTDIR}$(MISC_PATH)
	cp -r dictionaries/ $${DESTDIR}$(MISC_PATH)
	cp hook_socket.so /usr/local/lib/
	sudo ldconfig

publish: clean
	test "`basename $$PWD`" = "AFL" || exit 1
	test -f ~/www/afl/releases/$(PROGNAME)-$(VERSION).tgz; if [ "$$?" = "0" ]; then echo; echo "Change program version in config.h, mmkay?"; echo; exit 1; fi
	cd ..; rm -rf $(PROGNAME)-$(VERSION); cp -pr $(PROGNAME) $(PROGNAME)-$(VERSION); \
	  tar -cvz -f ~/www/afl/releases/$(PROGNAME)-$(VERSION).tgz $(PROGNAME)-$(VERSION)
	chmod 644 ~/www/afl/releases/$(PROGNAME)-$(VERSION).tgz
	( cd ~/www/afl/releases/; ln -s -f $(PROGNAME)-$(VERSION).tgz $(PROGNAME)-latest.tgz )
	cat docs/README >~/www/afl/README.txt
	cat docs/status_screen.txt >~/www/afl/status_screen.txt
	cat docs/historical_notes.txt >~/www/afl/historical_notes.txt
	cat docs/technical_details.txt >~/www/afl/technical_details.txt
	cat docs/ChangeLog >~/www/afl/ChangeLog.txt
	cat docs/QuickStartGuide.txt >~/www/afl/QuickStartGuide.txt
	echo -n "$(VERSION)" >~/www/afl/version.txt

.PHONY: clean all install publish test_x86 test_build all_done
