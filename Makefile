CXX ?= g++
CXXFLAGS ?= -O2 -Wall -std=c++11
LIBS = -lfreeglut -lopengl32 -lglu32 -lwinmm
TARGET = game.exe

SRCS = main.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET) $(LIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
