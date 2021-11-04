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

LDFLAGS+= -lgtest

ifeq ($(MUSL_BUILD),1)
OUTDIR=../../bin/musl
else
OUTDIR=../../bin/glibc
endif
OUT=$(OUTDIR)/unit_tests

OBJS = \
    $(OUTDIR)/tests_main.o

HEADERS = \
    ../cmonitor.h
    

# Targets

all: $(OUT)

test: $(OUT)
	$(OUT)

$(OUT): $(OBJS) $(ROOT_DIR)/Constants.mk
	$(CXX) $(LDFLAGS) -o $(OUT) $(OBJS)

clean:
	rm -f $(OUT) $(OBJS) *.err *.json *.log


# Rules

$(OUTDIR)/%.o: %.cpp $(HEADERS) $(ROOT_DIR)/Constants.mk
	@mkdir -p $(OUTDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $< 
