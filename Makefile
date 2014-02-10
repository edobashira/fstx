all: fstdisambiguate

CXXFLAGS=-stdlib=libstdc++
LDFLAGS=-stdlib=libstdc++

fstdisambiguate : fstdisambiguate.o
	$(CXX)  $^ -o $@  $(LDFLAGS) $(LDLIBS) -lfst 

clean:
	rm -rf *.o fstdisambiguate

%.o:%.cc ${includes}
	$(CXX) $(CXXFLAGS) -c $<

%.o:%.cpp ${includes}
	$(CXX) $(CXXFLAGS) -c $<
