CXXFLAGS=-g -O0 -Wall -Werror -std=c++11
OUT=njmon_collector

#OBJS =  \
    njmon_linux_v22.o \
    njmon_cgroups.o

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
