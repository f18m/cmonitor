CFLAGS=-g -O0
CXXFLAGS=-g -O0 -Wall
LDFLAGS=-g
OUT=njmon

OBJS =  \
 njmon_linux_v22.o \
 njmon_cgroups.o

$(OUT): $(OBJS)
	g++ $(CFLAGS) -o $(OUT) $(OBJS) $(LDFLAGS)

clean:
	rm -f $(OUT) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $< 

%.o: %.cpp
	$(CC) $(CXXFLAGS) -c -o $@ $< 
