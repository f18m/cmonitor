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
	mkdir -p $(DESTDIR)/usr/local/bin
	cp -f $(OUT) $(DESTDIR)/usr/local/bin/

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $< 

%.o: %.cpp
	$(CC) $(CXXFLAGS) -c -o $@ $< 
