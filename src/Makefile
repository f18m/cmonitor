CFLAGS=-g -O0
CXXFLAGS=-g -O0 -Wall -std=c++11
OUT=njmon_collector

OBJS =  \
    njmon_linux_v22.o \
    njmon_cgroups.o

$(OUT): $(OBJS)
	$(CXX) $(LDFLAGS) -o $(OUT) $(OBJS)

clean:
	rm -f $(OUT) $(OBJS)
	
install:
	mkdir -p $(DESTDIR)/$(BINDIR)/
	cp -f $(OUT) $(DESTDIR)/$(BINDIR)/

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $< 

%.o: %.cpp
	$(CC) $(CXXFLAGS) -c -o $@ $< 
