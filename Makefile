# Cockpit Makefile — ND + FMC 可移植子程序
#
# 编译目标:
#   make             编译 Cockpit.exe (独立可执行程序)
#   make lib          编译 libcockpit.a (嵌入项目的静态库)
#   make run          编译并运行
#   make clean        清理

CXX      := g++
CC       := gcc
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
CFLAGS   := -std=c11 -w
LDFLAGS  := -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -lSDL2_image -lSDL2_gfx -lws2_32
INCLUDES := -IND -IFMC -I.
AR       := ar rcs

TARGET   := Cockpit.exe
LIBNAME  := libcockpit.a
BUILD    := build

# === 源文件 ===
ND_C_SRC := ND/xplaneConnect.c
FMC_SRC  := FMC/fmc_buttons.cpp FMC/fmc_buttons_init.cpp \
            FMC/fmc_pages.cpp FMC/fmc_route.cpp FMC/fmc_deparr.cpp \
            FMC/fmc_data.cpp
PORTABLE_SRC := cockpit.cpp
MAIN_SRC := cockpit_main.cpp

# === 对象文件 ===
ND_C_OBJ  := $(patsubst %.c,$(BUILD)/%.o,$(ND_C_SRC))
FMC_OBJ   := $(patsubst %.cpp,$(BUILD)/%.o,$(FMC_SRC))
PORTABLE_OBJ := $(BUILD)/cockpit.o
MAIN_OBJ  := $(BUILD)/cockpit_main.o
EXE_OBJ   := $(MAIN_OBJ) $(PORTABLE_OBJ) $(ND_C_OBJ) $(FMC_OBJ)
LIB_OBJ   := $(PORTABLE_OBJ) $(ND_C_OBJ) $(FMC_OBJ)

.PHONY: all lib clean run help

all: $(TARGET)

$(TARGET): $(EXE_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "========================================"
	@echo "  Cockpit.exe 编译完成!"
	@echo "========================================"

lib: $(LIBNAME)

$(LIBNAME): $(LIB_OBJ)
	$(AR) $@ $^
	@echo "========================================"
	@echo "  libcockpit.a 编译完成!"
	@echo "  链接方式: -lSDL2 -lSDL2_ttf -lSDL2_image"
	@echo "  头文件: #include \"cockpit.h\""
	@echo "========================================"

# === 主程序入口 ===
$(BUILD)/cockpit_main.o: cockpit_main.cpp cockpit.h cockpit_internal.h
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# === 可移植子程序实现 ===
$(BUILD)/cockpit.o: cockpit.cpp cockpit.h cockpit_internal.h
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
	rm -rf $(BUILD) $(TARGET) $(LIBNAME)

run: $(TARGET)
	./$(TARGET)

help:
	@echo "make          — 编译 Cockpit.exe (独立程序)"
	@echo "make lib      — 编译 libcockpit.a (静态库)"
	@echo "make run      — 编译并运行"
	@echo "make clean    — 清理"
	@echo ""
	@echo "=== 集成到其他项目 ==="
	@echo "  头文件: #include \"cockpit.h\""
	@echo "  链接: -lSDL2 -lSDL2_ttf -lSDL2_image -lSDL2_gfx"
	@echo "  详见 PORTING.md"
