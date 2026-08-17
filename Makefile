CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
LDFLAGS := -lstdc++fs

# Directories
SRC_DIR := src
INC_DIR := include
BUILD_DIR := build
BIN_DIR := .

# Sources and objects
SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SOURCES))
DEPS := $(OBJECTS:.o=.d)

# Main target
TARGET := $(BIN_DIR)/vmfpropmerger

# Default target
all: $(TARGET)

# Build target
$(TARGET): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: $@"

# Compile objects
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -MMD -c $< -o $@

# Include dependency files
-include $(DEPS)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET)
	@echo "Clean complete"

# Rebuild
rebuild: clean all

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

.PHONY: all clean rebuild