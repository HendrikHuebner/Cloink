SRC_DIR = ./src
BUILD_DIR = ./build

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
HDRS = $(wildcard $(SRC_DIR)/*.hpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRCS))

SANITIZER = "-fsanitize=address"
CXX = clang++
CXXFLAGS = -Wall -Wextra -Wno-unused-parameter -std=c++20 -O3
DEBUGFLAGS = -g3 -O0 $(SANITIZER)

LLVM_CONFIG := llvm-config
LLVM_CPPFLAGS := $(shell $(LLVM_CONFIG) --cppflags)
LLVM_LDFLAGS := $(shell $(LLVM_CONFIG) --ldflags)
LLVM_LIBS := $(shell $(LLVM_CONFIG) --libs)

TARGET = clonk

all: $(TARGET)

$(TARGET): $(OBJS) $(HDRS)
	$(CXX) $(OBJS) $(LLVM_LDFLAGS) $(SANITIZER) $(LLVM_LIBS) -o $(TARGET)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(LLVM_CPPFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)/*.o $(TARGET)

debug: CXXFLAGS += $(DEBUGFLAGS)
debug: all

.PHONY: all clean debug
