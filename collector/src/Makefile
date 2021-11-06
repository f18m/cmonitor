#
# Main makefile to build cmonitor_collector
#

THIS_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR:=$(shell readlink -f $(THIS_DIR)/../..)

CXXFLAGS=-Wall -Werror -Wno-switch-bool -std=c++11 -DVERSION_STRING=\"$(RPM_VERSION)-$(RPM_RELEASE)\"

ifeq ($(DEBUG),1)
CXXFLAGS+=-g -O0    #useful for debugging
LDFLAGS+=-g -O0    #useful for debugging
else
CXXFLAGS+=-g -O2     # release mode; NOTE: without -g the creation of debuginfo RPMs will fail in COPR!
LDFLAGS+=-g -O2
endif

ifeq ($(MUSL_BUILD),1)
OUTDIR=../bin/musl
else
OUTDIR=../bin/glibc
endif
OUT=$(OUTDIR)/cmonitor_collector

VALGRIND_LOGFILE_POSTFIX:=${OUT}-$(shell date +%F-%H%M%S)
VALGRIND_COMMON_OPTS:=--gen-suppressions=all --time-stamp=yes --error-limit=no

ifeq ($(CMONITOR_ARGS),)
CMONITOR_ARGS:=--num-samples=10 --sampling-interval=1 --foreground --debug --collect=all --remote-ip=localhost --remote-port=8086
endif


# NOTE: --track-origins=yes makes Valgrind VERY slow but without it Valgrind will report false memory leaks
MEMCHECK_COMMON_OPTS:=--tool=memcheck --track-origins=yes --malloc-fill=AF --free-fill=BF --leak-check=full


OBJS = \
    $(OUTDIR)/cgroups.o \
    $(OUTDIR)/header_info.o \
    $(OUTDIR)/logger.o \
    $(OUTDIR)/main.o \
    $(OUTDIR)/output_frontend.o \
    $(OUTDIR)/system.o \
    $(OUTDIR)/utils.o

HEADERS = \
	cgroups.h \
	header_info.h \
	logger.h \
    cmonitor.h \
    output_frontend.h \
	system.h \
	utils.h \
    influxdb.h
    

# Targets

all: $(OUT)
	$(MAKE) -C tests all

test:
	$(MAKE) -C tests test

$(OUT): $(OBJS) $(ROOT_DIR)/Constants.mk
	$(CXX) $(LDFLAGS) -o $(OUT) $(OBJS)

clean:
	rm -f $(OUT) $(OBJS) *.err *.json *.log
	$(MAKE) -C tests clean

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
