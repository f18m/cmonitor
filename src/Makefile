CFLAGS=-g -O0
CXXFLAGS=-g -O0 -Wall -std=c++11
OUT=njmon

OBJS =  \
 njmon_linux_v22.o \
 njmon_cgroups.o

$(OUT): $(OBJS)
	g++ $(LDFLAGS) -o $(OUT) $(OBJS)

clean:
	rm -f $(OUT) $(OBJS)
	
install:
ifndef DESTDIR
	@echo "*** ERROR: please call this makefile supplying explicitly the DESTDIR variable"
	@exit 1
endif
	mkdir -p $(DESTDIR)/bin
	cp -f $(OUT) $(DESTDIR)/bin/

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $< 

%.o: %.cpp
	$(CC) $(CXXFLAGS) -c -o $@ $< 
