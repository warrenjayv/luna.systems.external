# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Isources
LDFLAGS = -lws2_32

# Target executable name and object paths
TARGET = nelly.exe
OBJS = main.o sources/command.o sources/process.o sources/network.o

# Default build rule
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS) 2> error

main.o: main.cpp sources/command.h
	$(CXX) $(CXXFLAGS) -c main.cpp 2> error

sources/command.o: sources/command.cpp sources/command.h sources/process.h sources/network.h
	$(CXX) $(CXXFLAGS) -c sources/command.cpp -o sources/command.o 2> error

sources/process.o: sources/process.cpp sources/process.h
	$(CXX) $(CXXFLAGS) -c sources/process.cpp -o sources/process.o 2> error

sources/network.o: sources/network.cpp sources/network.h
	$(CXX) $(CXXFLAGS) -c sources/network.cpp -o sources/network.o 2> error

# Clean up built files, objects, dump, and error logs
clean:
	rm -f *.o sources/*.o $(TARGET) dump error

.PHONY: all clean