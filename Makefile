# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -Wall -Wextra -std=c++20 -Iinclude -pedantic

# Ensure object directories exist
$(shell mkdir -p obj)

# Source files
SRCS = $(wildcard src/*.cpp)

# Header files
HDRS = $(wildcard include/*.hpp)

# Object files
OBJS = $(patsubst src/%.cpp,obj/%.o,$(SRCS))

# Executable names
TARGET = ipk25-chat

# Default target
all: $(TARGET)

# Main executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile src files into obj//
obj/%.o: src/%.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -f $(OBJS) $(ARGOBJS) $(TARGET)
	rm -rf obj/*

ifeq ($(shell uname), Darwin)
testServerTcp:
	nc -l 4567 -k -c
else
testServerTcp:
	nc -l -k -p 4567 -C
endif

testServerUDP:
	python3 udpServer.py

umlDiagram:
	hpp2plantuml -i "./include/*.hpp" -o output.puml
	plantuml -tsvg output.puml
	rm output.puml

.PHONY: all clean argTest zip valgrind rebuild testTcp testUDP umlDiagram

