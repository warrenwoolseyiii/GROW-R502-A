# Makefile for R502-A C++ Driver Example

# Compiler and flags
# On macOS, g++ is often an alias for clang++. Let's be explicit.
CXX = clang++
CC = clang # Also use clang for C files for consistency on macOS
CXXFLAGS = -Wall -Wextra -g -std=c++11 -I../../lib/Cpp -I../../lib/C
# CFLAGS_C_COMPAT will also use clang via CC variable
CFLAGS_C_COMPAT = -Wall -Wextra -g -I../../lib/C
LDFLAGS =

# Source files
# C library source (needed by C++ library if it wraps C calls, or if C++ directly uses C structs/enums)
C_LIB_SRCS = ../../lib/C/r502a_driver.c
# C++ library sources
CPP_LIB_SRCS = ../../lib/Cpp/FingerprintSensor.cpp
# Example source
EXAMPLE_SRCS = main.cpp

# Build directory
BUILD_DIR = build

# Object files (prefixed with build directory, using notdir to get base filenames)
C_LIB_OBJS = $(BUILD_DIR)/$(notdir $(C_LIB_SRCS:.c=.o))
CPP_LIB_OBJS = $(BUILD_DIR)/$(notdir $(CPP_LIB_SRCS:.cpp=.o))
EXAMPLE_OBJS = $(BUILD_DIR)/$(notdir $(EXAMPLE_SRCS:.cpp=.o))

OBJS = $(EXAMPLE_OBJS) $(CPP_LIB_OBJS) $(C_LIB_OBJS)

# Target executable (in build directory)
TARGET_NAME = r502a_cpp_example
TARGET = $(BUILD_DIR)/$(TARGET_NAME)

.PHONY: all clean

all: $(TARGET)

# Create build directory if it doesn't exist
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Link the target executable
$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Define CFLAGS_C_COMPAT for compiling the C file (r502a_driver.c)
CFLAGS_C_COMPAT = -Wall -Wextra -g -I../../lib/C

# Rule to compile C library source file into the build directory
$(BUILD_DIR)/%.o: ../../lib/C/%.c ../../lib/C/r502a_driver.h | $(BUILD_DIR)
	$(CC) $(CFLAGS_C_COMPAT) -c $< -o $@

# Rule to compile C++ library source files into the build directory
# Need to handle specific source file for specific object file
$(BUILD_DIR)/FingerprintSensor.o: ../../lib/Cpp/FingerprintSensor.cpp ../../lib/Cpp/FingerprintSensor.hpp ../../lib/C/r502a_driver.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c ../../lib/Cpp/FingerprintSensor.cpp -o $@

# Rule to compile example C++ source files into the build directory
$(BUILD_DIR)/main.o: main.cpp ../../lib/Cpp/FingerprintSensor.hpp ../../lib/C/r502a_driver.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c main.cpp -o $@

clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR)