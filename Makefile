CXX ?= g++
CXXFLAGS ?= -O2 -Wall -std=c++11 -Iinclude
LIBS = -static-libgcc -static-libstdc++ -lfreeglut -lopengl32 -lglu32 -lwinmm
TARGET = game.exe

SRCS = main.cpp src/Audio/SoundEngine.cpp src/Graphics/Renderer.cpp src/Graphics/ParticleSystem.cpp src/Graphics/Starfield.cpp src/Graphics/CGLab.cpp src/Entities/Powerup.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET) $(LIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
