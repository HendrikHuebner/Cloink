SRC_DIR = ./src
BUILD_DIR = ./build

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRCS))

CXX = clang++
CXXFLAGS = -Wall -Wextra -O2
DEBUGFLAGS = -g -O0

TARGET = $(BUILD_DIR)/bc0

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)/*.o $(TARGET)

debug: CXXFLAGS += $(DEBUGFLAGS)
debug: all

.PHONY: all clean debug
