# Cockpit 子程序移植与集成指南

## 概述

Cockpit 是一个 ND(导航显示器) + FMC(飞行管理计算机) 综合座舱模拟程序，
已封装为可嵌入其他项目的子程序库。

## 文件结构

```
ND_FMC/
├── cockpit.h            ★ 唯一需要 #include 的公共头文件
├── cockpit_internal.h     内部实现头文件 (平台适配)
├── cockpit.cpp            可移植实现 (PIMPL 模式)
├── cockpit_main.cpp       独立可执行程序入口
├── shared_mem.h           共享内存 IPC (Windows only)
├── fmc_shm_sync.h         共享内存同步回调
├── Makefile               编译系统
├── PORTING.md             本文档
├── ND/                    ND 导航显示器模块
│   ├── xplaneConnect.h/c  X-Plane UDP 通信库
│   ├── nd_xpc.h           X-Plane 数据接口
│   ├── nd_thread.h        数据采集线程
│   ├── nd_map.h           地图渲染
│   ├── renderer.h         SDL 渲染器
│   ├── navdata.h          导航数据
│   ├── nd_data.h          文件模式数据
│   ├── navaid_hash.h      导航台哈希表
│   └── config.h           颜色/尺寸常量
├── FMC/                   FMC 飞行管理计算机模块
│   ├── fmc_pages.h/cpp    页面绘制
│   ├── fmc_route.h/cpp    航路管理 (AVL树)
│   ├── fmc_buttons.cpp    按键初始化
│   ├── fmc_buttons_init.cpp
│   ├── fmc_ui.h           用户界面
│   ├── fmc_deparr.h/cpp   进离场数据层
│   ├── fmc_data.h/cpp     进离场查询数据
│   ├── renderer.h         SDL 渲染器
│   └── config.h           颜色常量
└── assets/                资源文件
    ├── KSEAKBFI.fms       飞行计划
    ├── nd.dat             模拟飞行数据
    ├── earth_nav.dat      导航台数据
    ├── earth_fix.dat      航路点数据
    ├── apt.dat            机场数据
    └── *.ttf              字体文件
```

## 快速集成 (3 步)

### 方式 A: 静态库链接

```bash
# 1. 编译静态库
mingw32-make lib    # 生成 libcockpit.a

# 2. 在你的项目中链接
g++ your_app.cpp -I/path/to/ND_FMC -L/path/to/ND_FMC -lcockpit \
    -lSDL2 -lSDL2_ttf -lSDL2_image -lSDL2_gfx -lws2_32 \
    -o your_app
```

### 方式 B: 源码集成

将 `cockpit.h`, `cockpit.cpp`, `ND/`, `FMC/`, `assets/`,
`shared_mem.h`, `fmc_shm_sync.h` 复制到你的项目，
添加所有 `.cpp`/`.c` 文件到编译列表即可。

## API 使用示例

```cpp
#include "cockpit.h"

// 方式1: 阻塞式运行 (最简单)
int main() {
    CockpitContext ctx;
    ctx.init();      // 初始化 SDL + ND + FMC + X-Plane
    ctx.run();       // 阻塞直到用户关闭窗口
    ctx.cleanup();   // 清理
}

// 方式2: 异步启动 (嵌入 GUI 程序)
void start_cockpit_in_background() {
    static CockpitContext ctx;
    ctx.init();
    ctx.start();  // 后台线程, 立即返回
}

// 方式3: 手动帧循环 (游戏引擎集成)
void game_loop() {
    CockpitContext ctx;
    ctx.init();
    while (game_running) {
        if (!ctx.poll_frame()) break;  // 处理一帧
    }
    ctx.cleanup();
}

// 方式4: 查询飞行状态 (线程安全)
CockpitFlightState state = ctx.get_flight_state();
printf("LAT:%.4f LON:%.4f HDG:%.0f GS:%.0fkt XPlane:%s\n",
    state.lat, state.lon, state.heading, state.gs,
    state.xplane_connected ? "ON" : "OFF");
```

## 依赖库

| 库 | 版本 | 用途 |
|---|------|------|
| SDL2 | ≥2.0 | 窗口/图形 |
| SDL2_ttf | ≥2.0 | 字体渲染 |
| SDL2_image | ≥2.0 | 图片加载 |
| SDL2_gfx | ≥1.0 | 几何图形 |
| C++17 | - | 标准库 |

Windows 额外: `ws2_32` (X-Plane UDP通信)

## 平台支持

| 特性 | Windows | Linux/macOS |
|------|---------|-------------|
| ND 渲染 | ✅ | ⚠️ 需 SDL2 |
| FMC 交互 | ✅ | ⚠️ 需 SDL2 |
| X-Plane 连接 | ✅ | ⚠️ 需调整 socket |
| 共享内存 IPC | ✅ | ❌ Windows only |
| 多线程 | ✅ | ⚠️ 需 pthread |

> ⚠️ = 代码已预留接口(cockpit_internal.h)，需要验证

## 自定义资产路径

```cpp
CockpitContext ctx;
ctx.set_asset_path("D:/my_project/data/");  // 在 init() 前调用
ctx.init();
```

## API 参考

详见 `cockpit.h` 头文件中的完整注释。

### 生命周期
- `init()` → `run()` / `start()` / `poll_frame()` → `cleanup()`

### 状态查询 (线程安全)
- `get_flight_state()` — 飞行数据快照
- `get_route_count()` / `get_waypoint(idx)` — 航路点
- `get_origin()` / `get_destination()` — 起降机场
- `is_xplane_connected()` — X-Plane 在线状态
