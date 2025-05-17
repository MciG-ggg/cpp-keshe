CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I./src/backend/include
LDFLAGS = -pthread -lstdc++fs

SRC_DIR = src/backend
OBJ_DIR = obj

SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
TARGET = parking_api_server

.PHONY: all clean run

all: $(OBJ_DIR) $(TARGET)

run: all
	@echo "Starting parking management system..."
	./$(TARGET)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)