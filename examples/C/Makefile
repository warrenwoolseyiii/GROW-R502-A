# Makefile for R502-A C Driver Example

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g -I../../lib/C
LDFLAGS =

# Source files
# Assuming r502a_driver.c is in ../../lib/C/
LIB_SRCS = ../../lib/C/r502a_driver.c
EXAMPLE_SRCS = main.c

# Build directory
BUILD_DIR = build

# Object files (prefixed with build directory)
LIB_OBJS = $(BUILD_DIR)/$(notdir $(LIB_SRCS:.c=.o))
EXAMPLE_OBJS = $(BUILD_DIR)/$(notdir $(EXAMPLE_SRCS:.c=.o))
OBJS = $(EXAMPLE_OBJS) $(LIB_OBJS)

# Target executable (in build directory)
TARGET_NAME = r502a_c_example
TARGET = $(BUILD_DIR)/$(TARGET_NAME)

.PHONY: all clean

all: $(TARGET)

# Create build directory if it doesn't exist
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Link the target executable
$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Rule to compile library source files into the build directory
$(BUILD_DIR)/%.o: ../../lib/C/%.c ../../lib/C/r502a_driver.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to compile example source files into the build directory
$(BUILD_DIR)/%.o: %.c ../../lib/C/r502a_driver.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR)