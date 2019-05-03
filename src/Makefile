#
# Main makefile to build njmon_collector
#

THIS_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

CXXFLAGS=-Wall -Werror -std=c++11 -DVERSION_STRING=\"$(RPM_VERSION)-$(RPM_RELEASE)\"
#CXXFLAGS+=-g -O0    #useful for debugging
CXXFLAGS+=-g -O2     # release mode; NOTE: without -g the creation of debuginfo RPMs will fail in COPR!
OUT=njmon_collector

VALGRIND_LOGFILE_POSTFIX:=${OUT}-$(shell date +%F-%H%M%S)
VALGRIND_COMMON_OPTS:=--gen-suppressions=all --time-stamp=yes --error-limit=no

# NOTE: --track-origins=yes makes Valgrind VERY slow but without it Valgrind will report false memory leaks
MEMCHECK_COMMON_OPTS:=--tool=memcheck --track-origins=yes --malloc-fill=AF --free-fill=BF --leak-check=full


OBJS = \
    njmon_main.o \
    njmon_cgroups.o \
    njmon_json.o \
    njmon_proc_stats.o \
    njmon_utils.o


# Targets

$(OUT): $(OBJS)
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
			$(THIS_DIR)/$(OUT) --num-samples=10 --sampling-interval=1 --foreground --debug
	@echo "Valgrind exited"


# Rules

%.o: %.cpp njmon.h
	$(CXX) $(CXXFLAGS) -c -o $@ $< 
