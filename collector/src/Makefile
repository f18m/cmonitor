#
# Main makefile to build cmonitor_collector
#

THIS_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

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
    cgroups.o \
    header_info.o \
    main.o \
    output_frontend.o \
    proc_stats.o \
    utils.o
    
HEADERS = \
    cmonitor.h \
    output_frontend.h \
    influxdb.h
    

# Targets

$(OUT): $(OBJS)
	mkdir -p $(OUTDIR)
	$(CXX) $(LDFLAGS) -o $(OUT) $(OBJS)

clean:
	rm -f $(OUT) $(OBJS) *.err *.json *.log
	
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

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c -o $@ $< 
