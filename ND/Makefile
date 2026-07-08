CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
LDFLAGS  := -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -lSDL2_image -lSDL2_gfx -lws2_32
INCLUDES := -I. -I..

TARGET   := ND.exe
SOURCES  := main.cpp xplaneConnect.c
OBJECTS  := build/main.o build/xplaneConnect.o

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS)

build/main.o: main.cpp config.h nd_map.h navdata.h nd_data.h nd_xpc.h renderer.h navaid_hash.h nd_thread.h ../shared_mem.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c main.cpp -o $@

build/xplaneConnect.o: xplaneConnect.c xplaneConnect.h
	@mkdir -p build
	gcc -std=c11 -w -c xplaneConnect.c -o $@

clean:
	rm -rf build $(TARGET)

run: $(TARGET)
	./$(TARGET)
