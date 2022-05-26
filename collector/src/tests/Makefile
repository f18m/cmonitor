#
# Main makefile to build cmonitor_collector unit tests
#

THIS_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR:=$(shell readlink -f $(THIS_DIR)/../../..)

CXXFLAGS=-Wall -Werror -Wno-switch-bool -std=c++11 -fPIC -DVERSION_STRING=\"$(RPM_VERSION)-$(RPM_RELEASE)\"

ifeq ($(DEBUG),1)
CXXFLAGS += -g -O0   #useful for debugging
LDFLAGS += -g -O0    #useful for debugging
else
CXXFLAGS += -g -O2   # release mode; NOTE: without -g the creation of debuginfo RPMs will fail in COPR!
LDFLAGS += -g -O2
endif
LIBS += -lfmt -lgtest -lpthread

ifeq ($(PROMETHEUS_SUPPORT),)
LDFLAGS += -Wl,--rpath,/usr/lib64
LIBS += -lprometheus-cpp-pull -lprometheus-cpp-core -lz
CXXFLAGS += -DPROMETHEUS_SUPPORT=1
endif


ifeq ($(MUSL_BUILD),1)
OUTDIR=../../bin/musl
else
OUTDIR=../../bin/glibc
endif
OUT=$(OUTDIR)/unit_tests

OBJS_UNIT_TESTS = \
    $(OUTDIR)/tests_cgroup.o \
    $(OUTDIR)/tests_fast_file_reader.o \
    $(OUTDIR)/tests_main.o \
	$(OUTDIR)/tests_utils_misc.o

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

OBJS = $(OBJS_UNIT_TESTS) $(OBJS_CMONITOR_COLLECTOR)
HEADERS = $(wildcard ../*.h)

TEST_KERNELS = \
	centos7-Linux-3.10.0-x86_64-docker \
	centos7-Linux-3.10.0-x86_64-systemd \
	fedora35-Linux-5.14.17-x86_64-docker \
	fedora35-Linux-5.14.17-x86_64-systemd \
	ubuntu20.04-Linux-5.4.0-x86_64-docker \
	ubuntu20.04-Linux-5.4.0-x86_64-systemd

# Targets

all: $(OUT)

test: $(OUT)
	@echo "Running unit test binary $(OUT)"
	ldd $(OUT)
	sudo $(OUT) $(ARGS)
tests: $(OUT)
	@echo "Running unit test binary $(OUT)"
	sudo $(OUT) $(ARGS)

debug:
	gdb $(OUT) $(ARGS)
test_debug:
	gdb $(OUT) $(ARGS)

diff_expected_vs_results:
	for kk in $(TEST_KERNELS); do \
		diff -U3 $$kk/expected-nothreads.json $$kk/result-nothreads.json ; \
		diff -U3 $$kk/expected-withthreads.json $$kk/result-withthreads.json ; \
	done
accept_new_results:
	for kk in $(TEST_KERNELS); do \
		cp $$kk/result-nothreads.json $$kk/expected-nothreads.json ; \
		cp $$kk/result-withthreads.json $$kk/expected-withthreads.json ; \
	done

$(OUT): $(OBJS) $(ROOT_DIR)/Constants.mk
	$(CXX) $(LDFLAGS) -o $(OUT) $(OBJS) $(LIBS)

clean:
	rm -f $(OUT) $(OBJS) *.err *.json *.log
	for kk in $(TEST_KERNELS); do \
		rm -rf $$kk/sample*/proc $$kk/sample*/sys $$kk/sample*/sample-timestamp ; \
		rm -f $$kk/current-sample ; \
		rm -f $$kk/result-*.json ; \
	done

# Rules

$(OUTDIR)/%.o: %.cpp $(HEADERS) $(ROOT_DIR)/Constants.mk
	@mkdir -p $(OUTDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $< 
