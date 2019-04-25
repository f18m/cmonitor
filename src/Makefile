#
# Main makefile to build njmon_collector
#

CXXFLAGS=-Wall -Werror -std=c++11 -DVERSION_STRING=\"$(RPM_VERSION)-$(RPM_RELEASE)\"
#CXXFLAGS+=-g -O0    #useful for debugging
CXXFLAGS+=-g -O2     # release mode; NOTE: without -g the creation of debuginfo RPMs will fail in COPR!
OUT=njmon_collector

OBJS =  \
    njmon_main.o \
    njmon_cgroups.o \
    njmon_json.o \
    njmon_proc_stats.o \
    njmon_utils.o
    
$(OUT): $(OBJS)
	$(CXX) $(LDFLAGS) -o $(OUT) $(OBJS)

clean:
	rm -f $(OUT) $(OBJS)
	
install:
	mkdir -p $(DESTDIR)/$(BINDIR)/
	cp -f $(OUT) $(DESTDIR)/$(BINDIR)/

%.o: %.cpp njmon.h
	$(CXX) $(CXXFLAGS) -c -o $@ $< 
