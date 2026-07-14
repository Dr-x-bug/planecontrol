/**
 * cockpit.h — Cockpit 子程序可移植公共 API
 *
 * ========== 概述 ==========
 * 将 ND(导航显示器) + FMC(飞行管理计算机) 综合座舱程序封装为
 * 可独立编译、可嵌入其他项目的子程序库。
 *
 * ========== 依赖 ==========
 *   - SDL2, SDL2_ttf, SDL2_image, SDL2_gfx (图形渲染)
 *   - C++17 标准库 (std::atomic, std::string, std::vector)
 *
 * ========== 使用方式 ==========
 *
 *   方式1 — 单线程阻塞 (最简单, 适合独立线程):
 *   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
 *   #include "cockpit.h"
 *   int main() {
 *       CockpitContext ctx;
 *       ctx.init();     // 初始化
 *       ctx.run();      // 阻塞直到用户关闭窗口
 *       ctx.cleanup();  // 清理
 *   }
 *   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *   方式2 — 异步启动/停止 (适合嵌入GUI程序):
 *   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
 *   CockpitContext ctx;
 *   ctx.init();
 *   ctx.start();       // 后台线程启动
 *   // ... 主线程做其他事 ...
 *   ctx.stop();        // 停止
 *   ctx.cleanup();
 *   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *   方式3 — 手动帧循环 (适合与游戏引擎集成):
 *   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
 *   CockpitContext ctx;
 *   ctx.init();
 *   while (running) {
 *       if (!ctx.poll_frame()) break;  // 处理一帧, 返回false=退出
 *   }
 *   ctx.cleanup();
 *   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * ========== 线程安全 ==========
 *   - init() / cleanup() 必须在同一线程中成对调用
 *   - start() / stop() / request_stop() 可从任意线程调用
 *   - 所有 get*() 查询接口均为线程安全
 *
 * ========== 移植指南 ==========
 *   将此文件(FMC/ND文件夹 + 所有cpp/h文件 + assets/)复制到目标项目,
 *   链接 SDL2 系列库, 配置头文件搜索路径包含 ND/ 和 FMC/ 目录即可。
 *   详见 PORTING.md
 */

#pragma once

#include <string>
#include <vector>
#include <cstring>

// ============================================================
// 前向声明 (不暴露内部实现)
// ============================================================
struct CockpitImpl;  // PIMPL 隐藏实现

// ============================================================
// 公开数据结构
// ============================================================

/** 航路点信息 (外部查询用) */
struct CockpitWaypoint {
    char   id[16];     // 标识符 (如 "KSEA", "BLI")
    double lat;        // 纬度 (deg)
    double lon;        // 经度 (deg)
    int    freq;       // 导航频率 (kHz×100)
    char   type;       // 'V'=VOR, 'N'=NDB, 'F'=FIX, 'A'=AIRPORT

    CockpitWaypoint() : id{}, lat(0), lon(0), freq(0), type(0) {}
};

/** 飞行状态快照 (外部查询用, 线程安全) */
struct CockpitFlightState {
    double  lat;              // 当前纬度 (deg)
    double  lon;              // 当前经度 (deg)
    double  heading;          // 航向 (deg)
    double  gs;               // 地速 (kt)
    double  tas;              // 真空速 (kt)
    double  alt;              // 气压高度 (ft)
    double  wind_dir;         // 风向 (deg)
    double  wind_spd;         // 风速 (kt)
    bool    xplane_connected; // X-Plane 在线
    bool    pos_valid;        // 位置数据有效
    int     frame_count;      // 数据帧序号

    CockpitFlightState() { memset(this, 0, sizeof(*this)); }
};

/** Cockpit 运行状态 */
enum class CockpitState {
    IDLE,      // 未初始化
    READY,     // 已初始化
    RUNNING,   // 运行中
    STOPPING,  // 正在停止
    STOPPED    // 已停止
};

// ============================================================
// CockpitContext — 座舱子程序上下文 (PIMPL)
// ============================================================
class CockpitContext {
public:
    CockpitContext();
    ~CockpitContext();

    // 禁止拷贝
    CockpitContext(const CockpitContext&) = delete;
    CockpitContext& operator=(const CockpitContext&) = delete;

    // ============================================================
    // 生命周期
    // ============================================================

    /**
     * 初始化所有模块 (SDL, ND窗口, FMC窗口, 导航数据库, X-Plane连接)
     * @return true=成功
     * @note  必须在最终调用 cleanup() 的同一线程中执行
     * @note  资产文件(assets/)必须位于工作目录下
     */
    bool init();

    /**
     * 阻塞式运行主循环
     * @note  SDL 事件循环必须在创建窗口的线程中运行
     * @note  循环直到用户关闭窗口/按ESC/调用 request_stop()
     */
    void run();

    /**
     * 单帧轮询 (非阻塞)
     * @return true=继续运行, false=用户请求退出
     * @note  适合嵌入游戏引擎的帧循环中
     */
    bool poll_frame();

    /**
     * 异步启动: 在后台线程中执行 run()
     * @return true=启动成功
     */
    bool start();

    /**
     * 请求停止 (非阻塞, 可从任意线程调用)
     */
    void request_stop();

    /**
     * 等待后台线程退出 (阻塞, 带超时)
     * @param timeout_ms 超时毫秒数, 0=无限等待
     * @return true=已退出
     */
    bool join(int timeout_ms = 0);

    /** 停止并等待 (request_stop + join) */
    void stop();

    /**
     * 清理所有资源
     * @note  必须与 init() 在同一线程中调用
     */
    void cleanup();

    // ============================================================
    // 状态查询 (线程安全)
    // ============================================================

    CockpitState        state()               const;
    bool                is_running()           const;
    bool                is_xplane_connected()  const;
    CockpitFlightState  get_flight_state()     const;
    int                 get_route_count()      const;
    CockpitWaypoint     get_waypoint(int idx)  const;
    std::string         get_origin()           const;
    std::string         get_destination()      const;
    std::string         get_flight_number()    const;
    int                 get_frame_count()      const;

    // ============================================================
    // 程序控制 (线程安全)
    // ============================================================

    /** 设置数据资产目录 (必须在 init() 前调用) */
    void set_asset_path(const std::string& path);

    /** 获取当前版本字符串 */
    static const char* version();

private:
    CockpitImpl* m_impl;  // PIMPL: 隐藏所有平台相关实现细节
};
