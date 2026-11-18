# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Isources -Iheaders
LDFLAGS = -lws2_32

# Target executable name and object paths
TARGET = nelly.exe
OBJS = main.o sources/command.o sources/process.o sources/network.o sources/resolver.o sources/conjure.o

# Default build rule
all: $(TARGET)
	@printf "\033[32mcomplete\033[0m\n"

$(TARGET): $(OBJS)
	@$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS) 2> error || (printf "\033[31mfail\033[0m\n"; exit 1)

main.o: main.cpp sources/command.h
	@$(CXX) $(CXXFLAGS) -c main.cpp 2> error || (printf "\033[31mfail\033[0m\n"; exit 1)

sources/command.o: sources/command.cpp sources/command.h sources/process.h sources/network.h
	@$(CXX) $(CXXFLAGS) -c sources/command.cpp -o sources/command.o 2> error || (printf "\033[31mfail\033[0m\n"; exit 1)

sources/process.o: sources/process.cpp sources/process.h sources/resolver.h
	@$(CXX) $(CXXFLAGS) -c sources/process.cpp -o sources/process.o 2> error || (printf "\033[31mfail\033[0m\n"; exit 1)

sources/network.o: sources/network.cpp sources/network.h
	@$(CXX) $(CXXFLAGS) -c sources/network.cpp -o sources/network.o 2> error || (printf "\033[31mfail\033[0m\n"; exit 1)

sources/resolver.o: sources/resolver.cpp sources/resolver.h
	@$(CXX) $(CXXFLAGS) -c sources/resolver.cpp -o sources/resolver.o 2> error || (printf "\033[31mfail\033[0m\n"; exit 1)

sources/conjure.o: sources/conjure.cpp sources/conjure.h
	@$(CXX) $(CXXFLAGS) -c sources/conjure.cpp -o sources/conjure.o 2> error || (printf "\033[31mfail\033[0m\n"; exit 1)

# Clean up built files, objects, dump directory, and error logs recursively
clean:
	@rm -rf *.o sources/*.o $(TARGET) dump error
	@printf "\033[32mclean complete\033[0m\n"

.PHONY: all clean