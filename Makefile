SRC_DIR = ./src
BUILD_DIR = ./build

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRCS))

CXX = g++
CXXFLAGS = -Wall -Wextra -fsanitize=undefined
DEBUGFLAGS = -g3 -O0 -fsanitize=undefined 

TARGET = $(BUILD_DIR)/bc0

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -fsanitize=undefined -o $(TARGET)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)/*.o $(TARGET)

debug: CXXFLAGS += $(DEBUGFLAGS)
debug: all

.PHONY: all clean debug
