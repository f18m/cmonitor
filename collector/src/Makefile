#
# Main makefile to build cmonitor_collector
#

THIS_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR:=$(shell readlink -f $(THIS_DIR)/../..)
include $(ROOT_DIR)/Constants.mk
include $(ROOT_DIR)/Prometheus-Support.mk

# IMPORTANT: we do require c++11, i.e. GCC 4.8.1 or higher;
#            Centos7 which is one of the oldest-supported distributions provides gcc 4.8.5 so we
#            cannot move to e.g. c++14/17/20 yet
# IMPORTANT#2: -fPIC is required to build on fedora-rawhide
CXXFLAGS += -Wall -Werror -Wno-switch-bool -std=c++11 -fPIC $(DEFS)

ifeq ($(DEBUG),1)
CXXFLAGS += -g -O0   #useful for debugging
LDFLAGS += -g -O0    #useful for debugging
else
CXXFLAGS += -g -O2   # release mode; NOTE: without -g the creation of debuginfo RPMs will fail in COPR!
LDFLAGS += -g -O2
endif
LIBS += -lfmt

ifeq ($(MUSL_BUILD),1)
OUTDIR=../bin/musl
else
OUTDIR=../bin/glibc
endif
OUT=$(OUTDIR)/cmonitor_collector

VALGRIND_LOGFILE_POSTFIX:=cmonitor_collector-$(shell date +%F-%H%M%S)
VALGRIND_COMMON_OPTS:=--gen-suppressions=all --time-stamp=yes --error-limit=no

ifeq ($(CMONITOR_ARGS),)
CMONITOR_ARGS:=--num-samples=10 --sampling-interval=1 --foreground --debug --collect=all --remote-ip=localhost --remote-port=8086
endif


# NOTE: --track-origins=yes makes Valgrind VERY slow but without it Valgrind will report false memory leaks
MEMCHECK_COMMON_OPTS:=--tool=memcheck --track-origins=yes --malloc-fill=AF --free-fill=BF --leak-check=full


OBJS = \
    $(OUTDIR)/cgroups_config.o \
	$(OUTDIR)/cgroups_cpuacct.o \
	$(OUTDIR)/cgroups_memory.o \
	$(OUTDIR)/cgroups_network.o \
	$(OUTDIR)/cgroups_processes.o \
	$(OUTDIR)/fast_file_reader.o \
    $(OUTDIR)/header_info.o \
    $(OUTDIR)/logger.o \
    $(OUTDIR)/main.o \
    $(OUTDIR)/prometheus_counter.o \
    $(OUTDIR)/prometheus_gauge.o \
    $(OUTDIR)/output_frontend.o \
    $(OUTDIR)/system_cpu.o \
    $(OUTDIR)/system_memory.o \
    $(OUTDIR)/system_disk.o \
    $(OUTDIR)/system_network.o \
    $(OUTDIR)/system.o \
    $(OUTDIR)/utils_files.o \
    $(OUTDIR)/utils_misc.o \
    $(OUTDIR)/utils_string.o

HEADERS = $(wildcard *.h)
    

# Targets

all: $(OUT)
ifneq ($(DISABLE_UNIT_TESTS_BUILD),1)
	$(MAKE) -C tests all
endif
ifneq ($(DISABLE_BENCHMARKS_BUILD),1)
	$(MAKE) -C benchmarks all
endif

test:
	$(MAKE) -C tests test ARGS=$(ARGS)
tests:
	$(MAKE) -C tests test ARGS=$(ARGS)
test_debug:
	$(MAKE) -C tests test_debug ARGS=$(ARGS)

benchmark:
	$(MAKE) -C benchmarks benchmarks
benchmarks:
	$(MAKE) -C benchmarks benchmarks

.PHONY: tests benchmarks


$(OUT): $(OBJS) $(ROOT_DIR)/Constants.mk
	$(CXX) $(LDFLAGS) -o $(OUT) $(OBJS) $(LIBS)

clean:
	rm -f $(OUT) $(OBJS) *.err *.json *.log
	$(MAKE) -C tests clean
	$(MAKE) -C benchmarks clean

strip:
	strip $(OUT)

install:
	@mkdir -p $(DESTDIR)/$(BINDIR)/
	@cp -fv $(OUT) $(DESTDIR)/$(BINDIR)/

valgrind:
	@echo "Starting valgrind on $(OUT) for about 10secs"
	valgrind $(MEMCHECK_COMMON_OPTS) \
			 $(VALGRIND_COMMON_OPTS) \
			--log-file=valgrind-$(VALGRIND_LOGFILE_POSTFIX).log \
			$(THIS_DIR)/$(OUT) \
			$(CMONITOR_ARGS)
	@echo "Valgrind exited"


# Rules

$(OUTDIR)/%.o: %.cpp $(HEADERS) $(ROOT_DIR)/Constants.mk
	@mkdir -p $(OUTDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $< 
