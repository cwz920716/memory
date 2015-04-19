CXXFLAGS = $(shell llvm-config --cxxflags) -fno-rtti

all: mannotate.so
lib: libtrack.a

MemoryAnnotator.o: MemoryAnnotator.cpp MemoryAnnotator.h Makefile
	$(CXX) $(CXXFLAGS) MemoryAnnotator.cpp -c -o $@
FunctionClone.o: FunctionClone.cpp MemoryAnnotator.h Makefile
	$(CXX) $(CXXFLAGS) FunctionClone.cpp -c -o $@
mannotate.so: MemoryAnnotator.o FunctionClone.o
	$(CXX) -shared $^ -o $@
libtrack.a: lib/mtrack.cpp lib/mtrack.h
	g++ lib/mtrack.cpp -c -fPIC -o lib/libtrack.a
clean:
	rm -f *.o *.so
