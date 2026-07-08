# Cockpit Makefile — ND + FMC 集成编译
CXX      := g++
CC       := gcc
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
CFLAGS   := -std=c11 -w
LDFLAGS  := -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -lSDL2_image -lSDL2_gfx -lws2_32
INCLUDES := -IND -IFMC -I.

TARGET   := Cockpit.exe
BUILD    := build

# ND 源文件
ND_C_SRC := ND/xplaneConnect.c

# FMC 源文件
FMC_SRC  := FMC/fmc_buttons.cpp FMC/fmc_buttons_init.cpp \
            FMC/fmc_pages.cpp FMC/fmc_route.cpp FMC/fmc_deparr.cpp

# 主程序
MAIN_SRC := cockpit_main.cpp

# 所有对象文件
ND_C_OBJ  := $(patsubst %.c,$(BUILD)/%.o,$(ND_C_SRC))
FMC_OBJ   := $(patsubst %.cpp,$(BUILD)/%.o,$(FMC_SRC))
MAIN_OBJ  := $(BUILD)/cockpit_main.o
ALL_OBJ   := $(MAIN_OBJ) $(ND_C_OBJ) $(FMC_OBJ)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(ALL_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "========================================"
	@echo "  Cockpit.exe 编译完成!"
	@echo "  ND + FMC + X-Plane Connect 已集成"
	@echo "========================================"

# 主程序
$(BUILD)/cockpit_main.o: cockpit_main.cpp \
		ND/config.h ND/renderer.h ND/navdata.h ND/nd_data.h \
		ND/nd_xpc.h ND/nd_map.h ND/navaid_hash.h ND/nd_thread.h \
		FMC/config.h FMC/renderer.h FMC/fmc_ui.h FMC/fmc_pages.h FMC/fmc_route.h \
		shared_mem.h fmc_shm_sync.h
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# ND C 源文件
$(BUILD)/ND/%.o: ND/%.c ND/xplaneConnect.h
	@mkdir -p $(BUILD)/ND
	$(CC) $(CFLAGS) -c $< -o $@

# FMC C++ 源文件
$(BUILD)/FMC/%.o: FMC/%.cpp
	@mkdir -p $(BUILD)/FMC
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET)

run: $(TARGET)
	./$(TARGET)
