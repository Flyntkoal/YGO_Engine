
CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# Project directories
CORE_DIR = ygopro-core
CORE_LIB_DIR = $(CORE_DIR)/bin/release

# Include directories
INCLUDES = -I$(CORE_DIR)

# Library configuration
LDFLAGS = -L$(CORE_LIB_DIR) \
          -Wl,-rpath,@executable_path/$(CORE_LIB_DIR)

LIBS = -locgcore

# Target executable
TARGET = interface-test

# Source files
SOURCES = interface-test.cpp
OBJECTS = $(SOURCES:.cpp=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) $(LIBS) -o $(TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET)

