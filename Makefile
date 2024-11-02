CXX = g++
CXXFLAGS = -O3 -Wall -Wextra -std=c++20
DEBUGFLAGS = -O0 -g3 -Wall -Wextra -Wpedantic -std=c++20
LOGLEVEL ?= 1

SRC = main.cpp lexer.cpp parser.cpp
OBJ = $(SRC:.cpp=.o)
TARGET = bc0
DEBUG_TARGET = bc0_debug

.PHONY: all clean debug

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -DLOGLEVEL=$(LOGLEVEL) -o build/$@ $(OBJ)

debug: CXXFLAGS := $(DEBUGFLAGS)
debug: LOGLEVEL := 3
debug: $(DEBUG_TARGET)

$(DEBUG_TARGET): $(OBJ)
	$(CXX) $(DEBUGFLAGS) -DLOGLEVEL=$(LOGLEVEL) -o build/$@ $(OBJ)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -DLOGLEVEL=$(LOGLEVEL) -c src/$< -o build/$@

clean:
	rm -f $(OBJ) $(TARGET) $(DEBUG_TARGET)
