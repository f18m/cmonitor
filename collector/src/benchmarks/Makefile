#
# Main makefile to build cmonitor_collector benchmark tests
#

THIS_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR:=$(shell readlink -f $(THIS_DIR)/../../..)

CXXFLAGS=-Wall -Werror -Wno-switch-bool -std=c++11 -DVERSION_STRING=\"$(RPM_VERSION)-$(RPM_RELEASE)\"

ifeq ($(DEBUG),1)
CXXFLAGS+=-g -O0    #useful for debugging
LDFLAGS+=-g -O0    #useful for debugging
else
CXXFLAGS+=-g -O2     # release mode; NOTE: without -g the creation of debuginfo RPMs will fail in COPR!
LDFLAGS+=-g -O2
endif

LIBRARIES+= -lbenchmark -lpthread

ifeq ($(MUSL_BUILD),1)
OUTDIR=../../bin/musl
else
OUTDIR=../../bin/glibc
endif
OUT=$(OUTDIR)/benchmark_tests

OBJS = \
    $(OUTDIR)/open_fopen_ifstream_benchmark.o \
    $(OUTDIR)/cgroups_config.o \
	$(OUTDIR)/cgroups_cpuacct.o \
	$(OUTDIR)/cgroups_memory.o \
	$(OUTDIR)/cgroups_processes.o \
    $(OUTDIR)/logger.o \
    $(OUTDIR)/output_frontend.o \
    $(OUTDIR)/fast_file_reader.o \
    $(OUTDIR)/utils.o

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
	$(CXX) $(LDFLAGS) -o $(OUT) $(OBJS) $(LIBRARIES)

clean:
	rm -f $(OUT) $(OBJS) 

# Rules

$(OUTDIR)/%.o: %.cpp $(HEADERS) $(ROOT_DIR)/Constants.mk
	@mkdir -p $(OUTDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $< 
