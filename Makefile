SRC_DIR = ./src
BUILD_DIR = ./build

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
HDRS = $(wildcard $(SRC_DIR)/*.hpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRCS))

SANITIZER = 
CXX = clang++
CXXFLAGS = -Wall -Wextra -Wno-unused-parameter -Wno-format-security -std=c++20 -O3
DEBUGFLAGS = -g3 -O0
ASANFLAGS = -g0 -O0 "-fsanitize=address"

LLVM_CONFIG := llvm-config
LLVM_CPPFLAGS := $(shell $(LLVM_CONFIG) --cppflags)
LLVM_LDFLAGS := $(shell $(LLVM_CONFIG) --ldflags)
LLVM_LIBS := $(shell $(LLVM_CONFIG) --libs)

TARGET = clonk

all: $(TARGET)

$(TARGET): $(OBJS) $(HDRS)
	$(CXX) $(OBJS) $(LLVM_LDFLAGS) $(LLVM_LIBS) -o $(TARGET)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(LLVM_CPPFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)/*.o $(TARGET)

debug: CXXFLAGS += $(DEBUGFLAGS)
debug: all

asan: CXXFLAGS += $(ASANFLAGS)
asan: LLVM_LDFLAGS += $(ASANFLAGS)
	
.PHONY: all clean debug asan
