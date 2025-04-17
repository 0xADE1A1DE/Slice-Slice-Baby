CC=gcc
CFLAGS=$(OPS) -O3 -g3 -Wall -Wextra -MMD -MP -I/usr/local/lib -I../include -Wno-unknown-pragmas -Wno-unused-function -Wno-write-strings #-fsanitize=address,undefined,leak
PTHREAD=-pthread
LIBS=-L../build -levsp -lm -lassemblyline $(PTHREAD) 
BUILD_DIR=../build
SRC_DIR=../src
TESTS_DIR=../tests

# Find all test executables under test directory
TEST_SRCS := $(wildcard $(TESTS_DIR)/*.c)
TESTS := $(patsubst $(TESTS_DIR)/%.c, $(BUILD_DIR)/%, $(TEST_SRCS))

# Find all source files recursively under src directory
SOURCES := $(wildcard $(SRC_DIR)/**/*.c)
# Create object files in the build directory mirroring the src directory structure
OBJECTS := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SOURCES))
# Create dependency files in the build directory
DEPS := $(OBJECTS:.o=.d)

# Default target
.DEFAULT_GOAL := all

# Rule to build all targets
all: lib $(TESTS)

# Rule to create the static library
lib: $(OBJECTS)
	ar rcs $(BUILD_DIR)/libevsp.a $(OBJECTS)

# Rule to compile C files into object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to build each test executable separately
$(BUILD_DIR)/%: $(TESTS_DIR)/%.c lib
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@ $(LIBS) $(PTHREAD)

# Clean target to remove build artifacts
clean:
	rm -rf $(BUILD_DIR)
