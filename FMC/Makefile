CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -w
LDFLAGS  := -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -lSDL2_image -lSDL2_gfx
INCLUDES := -I. -I..

TARGET   := FMC.exe
SOURCES  := main.cpp fmc_buttons.cpp fmc_buttons_init.cpp fmc_pages.cpp fmc_route.cpp fmc_deparr.cpp
OBJECTS  := $(patsubst %.cpp,build/%.o,$(SOURCES))

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS)

build/%.o: %.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf build $(TARGET)

run: $(TARGET)
	./$(TARGET)
