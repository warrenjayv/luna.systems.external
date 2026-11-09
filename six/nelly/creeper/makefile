# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# Target executable name and source file
TARGET = nelly.exe
SRC = main.cpp

# Default build rule
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

# Clean up built files
clean:
		rm -f $(TARGET)

.PHONY: all clean
