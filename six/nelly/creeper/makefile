# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LDFLAGS = -lws2_32

# Target executable name and object files
TARGET = nelly.exe
OBJS = main.o network.o

# Default build rule
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

main.o: main.cpp network.h
	$(CXX) $(CXXFLAGS) -c main.cpp

network.o: network.cpp network.h
	$(CXX) $(CXXFLAGS) -c network.cpp

# Clean up built files
clean:
	rm -f *.o $(TARGET) dump

.PHONY: all clean