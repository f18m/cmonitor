#
# Main makefile to build cmonitor_collector unit tests
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

LIBRARIES+= -lgtest -lpthread

ifeq ($(MUSL_BUILD),1)
OUTDIR=../../bin/musl
else
OUTDIR=../../bin/glibc
endif
OUT=$(OUTDIR)/unit_tests

OBJS = \
    $(OUTDIR)/tests_main.o \
    $(OUTDIR)/cgroups.o \
    $(OUTDIR)/logger.o \
    $(OUTDIR)/output_frontend.o \
    $(OUTDIR)/utils.o

HEADERS = $(wildcard ../*.h)

TEST_KERNELS = centos7-Linux-3.10.0-x86_64

# Targets

all: $(OUT)

test: $(OUT)
	@echo "Running unit test binary $(OUT)"
	sudo $(OUT)

$(OUT): $(OBJS) $(ROOT_DIR)/Constants.mk
	$(CXX) $(LDFLAGS) -o $(OUT) $(OBJS) $(LIBRARIES)

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
