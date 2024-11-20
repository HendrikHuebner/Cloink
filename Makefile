SRC_DIR = ./src
BUILD_DIR = ./build

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRCS))

CXX = g++
CXXFLAGS = -Wall -Wextra -Wno-unused-parameter -fsanitize=address -std=c++20
DEBUGFLAGS = -g3 -O0 -fsanitize=address 

TARGET = bc0

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -fsanitize=address -o $(TARGET)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)/*.o $(TARGET)

debug: CXXFLAGS += $(DEBUGFLAGS)
debug: all

.PHONY: all clean debug
