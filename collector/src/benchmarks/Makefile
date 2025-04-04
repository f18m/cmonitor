#
# Main makefile to build cmonitor_collector benchmark tests
#

THIS_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR:=$(shell readlink -f $(THIS_DIR)/../../..)
include $(ROOT_DIR)/Constants.mk
include $(ROOT_DIR)/Prometheus-Support.mk

CXXFLAGS=-Wall -Werror -Wno-switch-bool -std=c++14 -fPIC $(DEFS)

ifeq ($(DEBUG),1)
CXXFLAGS += -g -O0    #useful for debugging
LDFLAGS += -g -O0    #useful for debugging
else
CXXFLAGS += -g -O2     # release mode; NOTE: without -g the creation of debuginfo RPMs will fail in COPR!
LDFLAGS += -g -O2
endif
LIBS += -lfmt -lbenchmark -lpthread

ifeq ($(PROMETHEUS_SUPPORT),1)
LIBS += -lprometheus-cpp-pull -lprometheus-cpp-core -lz
CXXFLAGS += -DPROMETHEUS_SUPPORT=1
endif

ifeq ($(MUSL_BUILD),1)
OUTDIR=../../bin/musl
else
OUTDIR=../../bin/glibc
endif
OUT=$(OUTDIR)/benchmark_tests

OBJS_BENCHMARKS = \
    $(OUTDIR)/open_fopen_ifstream_benchmark.o 

OBJS_CMONITOR_COLLECTOR = \
    $(OUTDIR)/cgroups_config.o \
	$(OUTDIR)/cgroups_cpuacct.o \
	$(OUTDIR)/cgroups_memory.o \
	$(OUTDIR)/cgroups_network.o \
	$(OUTDIR)/cgroups_processes.o \
	$(OUTDIR)/fast_file_reader.o \
    $(OUTDIR)/logger.o \
    $(OUTDIR)/prometheus_counter.o \
    $(OUTDIR)/prometheus_gauge.o \
    $(OUTDIR)/output_frontend.o \
    $(OUTDIR)/system.o \
    $(OUTDIR)/system_network.o \
    $(OUTDIR)/system_cpu.o \
    $(OUTDIR)/utils_files.o \
    $(OUTDIR)/utils_misc.o \
    $(OUTDIR)/utils_string.o

OBJS = $(OBJS_BENCHMARKS) $(OBJS_CMONITOR_COLLECTOR)

HEADERS = $(wildcard ../*.h)


# Targets

all: $(OUT)

benchmark: $(OUT)
	@echo "Running benchmark binary $(OUT)"
	cpupower frequency-set --governor performance
	$(OUT)
	cpupower frequency-set --governor powersave
benchmarks: $(OUT)
	@echo "Running benchmark binary $(OUT)"
	cpupower frequency-set --governor performance
	$(OUT)
	cpupower frequency-set --governor powersave

debug:
	gdb $(OUT)

$(OUT): $(OBJS) $(ROOT_DIR)/Constants.mk
	$(CXX) $(LDFLAGS) -o $(OUT) $(OBJS) -Wl,--start-group $(LIBS) -Wl,--end-group

clean:
	rm -f $(OUT) $(OBJS) 

# Rules

$(OUTDIR)/%.o: %.cpp $(HEADERS) $(ROOT_DIR)/Constants.mk
	@mkdir -p $(OUTDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $< 
