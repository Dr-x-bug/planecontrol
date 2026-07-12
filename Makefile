# Cockpit Makefile — ND + FMC 集成编译
# 支持多线程子程序 API (cockpit_api.h / cockpit_api.cpp)
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

# Cockpit API 库 (可被外部项目链接)
API_SRC  := cockpit_api.cpp

# 主程序入口
MAIN_SRC := cockpit_main.cpp

# 所有对象文件
ND_C_OBJ  := $(patsubst %.c,$(BUILD)/%.o,$(ND_C_SRC))
FMC_OBJ   := $(patsubst %.cpp,$(BUILD)/%.o,$(FMC_SRC))
API_OBJ   := $(BUILD)/cockpit_api.o
MAIN_OBJ  := $(BUILD)/cockpit_main.o
ALL_OBJ   := $(MAIN_OBJ) $(API_OBJ) $(ND_C_OBJ) $(FMC_OBJ)

.PHONY: all clean run run-async help

all: $(TARGET)

$(TARGET): $(ALL_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "========================================"
	@echo "  Cockpit.exe 编译完成!"
	@echo "  ND + FMC + X-Plane Connect 已集成"
	@echo "  支持多线程子程序 API"
	@echo "========================================"

# 主程序入口 (全局变量定义 + main)
$(BUILD)/cockpit_main.o: cockpit_main.cpp cockpit_api.h \
		ND/config.h ND/renderer.h ND/navdata.h ND/nd_data.h \
		ND/nd_xpc.h ND/nd_map.h ND/navaid_hash.h ND/nd_thread.h \
		FMC/config.h FMC/renderer.h FMC/fmc_ui.h FMC/fmc_pages.h FMC/fmc_route.h \
		shared_mem.h fmc_shm_sync.h
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Cockpit API 实现 (多线程子程序封装)
$(BUILD)/cockpit_api.o: cockpit_api.cpp cockpit_api.h \
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

run-async: $(TARGET)
	./$(TARGET) --async

help:
	@echo "make          — 编译 Cockpit.exe"
	@echo "make run      — 编译并运行 (阻塞模式)"
	@echo "make run-async— 编译并运行 (异步模式, 演示多线程调用)"
	@echo "make clean    — 清理编译产物"
	@echo ""
	@echo "外部项目可链接 cockpit_api.o 使用多线程 API:"
