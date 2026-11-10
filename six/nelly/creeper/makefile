CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LDFLAGS = -lws2_32

TARGET = nelly.exe
OBJS = main.o command.o process.o network.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

main.o: main.cpp command.h
	$(CXX) $(CXXFLAGS) -c main.cpp

command.o: command.cpp command.h process.h network.h
	$(CXX) $(CXXFLAGS) -c command.cpp

process.o: process.cpp process.h
	$(CXX) $(CXXFLAGS) -c process.cpp

network.o: network.cpp network.h
	$(CXX) $(CXXFLAGS) -c network.cpp

clean:
	rm -f *.o $(TARGET) dump

.PHONY: all clean