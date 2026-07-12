/**
 * cockpit_api.h — Cockpit 多线程子程序 API
 *
 * 将 ND + FMC 综合座舱程序包装为可多线程调用的子程序。
 * 外部调用者可在任意线程中创建 CockpitContext 实例并启动座舱模拟。
 *
 * 使用方式:
 *   // 方式1: 阻塞式运行 (适合放在独立线程中)
 *   CockpitContext ctx;
 *   ctx.init();
 *   ctx.run();       // 阻塞直到用户关闭窗口
 *   ctx.cleanup();
 *
 *   // 方式2: 异步启动/停止 (适合主线程控制)
 *   CockpitContext ctx;
 *   ctx.init();
 *   ctx.start();     // 在后台线程中运行
 *   // ... 主线程做其他事 ...
 *   ctx.stop();      // 请求停止并等待线程退出
 *   ctx.cleanup();
 *
 * 线程安全保证:
 *   - init() / cleanup() 必须在同一线程中成对调用
 *   - start() / stop() 可从任意线程调用
 *   - 所有状态查询接口均为线程安全
 */

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <atomic>

// ============================================================
// 前向声明 (内部实现类型)
// ============================================================
struct NDRenderer;
struct FMCRenderer;
struct GridHashTable;
struct Waypoint;
struct SharedMemoryIPC;

// ============================================================
// 公开数据结构
// ============================================================

/** 航路点信息 (供外部查询) */
struct CockpitWaypoint {
    char   id[16];
    double lat;
    double lon;
    int    freq;
    char   type;       // 'V'=VOR, 'N'=NDB, 'F'=FIX, 'A'=AIRPORT
};

/** 飞行状态快照 (供外部查询) */
struct CockpitFlightState {
    double  lat;            // 当前纬度
    double  lon;            // 当前经度
    double  heading;        // 航向 (deg)
    double  gs;             // 地速 (kt)
    double  tas;            // 真空速 (kt)
    double  alt;            // 气压高度 (ft)
    double  wind_dir;       // 风向 (deg)
    double  wind_spd;       // 风速 (kt)
    bool    xplane_connected;  // X-Plane 连接状态
    bool    pos_valid;      // 位置数据有效
    int     frame_count;    // 数据帧计数

    CockpitFlightState() { memset(this, 0, sizeof(*this)); }
};

/** Cockpit 运行状态 */
enum class CockpitState {
    IDLE,           // 未初始化
    READY,          // 已初始化, 等待运行
    RUNNING,        // 正在运行
    STOPPING,       // 正在停止
    STOPPED         // 已停止
};

// ============================================================
// CockpitContext — 座舱子程序上下文
// ============================================================
class CockpitContext {
public:
    CockpitContext();
    ~CockpitContext();

    // ---- 禁止拷贝 ----
    CockpitContext(const CockpitContext&) = delete;
    CockpitContext& operator=(const CockpitContext&) = delete;

    // ========== 生命周期 ==========

    /**
     * 初始化所有模块 (SDL, ND, FMC, 共享内存, X-Plane连接)
     * @return true=成功, false=失败
     * @note  必须在调用线程中执行, 不可跨线程
     */
    bool init();

    /**
     * 阻塞式运行主循环
     * @note  在 Windows 上 SDL 事件循环必须在创建窗口的线程中运行
     * @note  循环直到用户关闭窗口或调用 request_stop()
     */
    void run();

    /**
     * 异步启动: 创建后台线程执行 run()
     * @return true=启动成功
     */
    bool start();

    /**
     * 请求停止主循环 (非阻塞)
     * @note  可在任意线程中调用
     */
    void request_stop();

    /**
     * 等待后台线程退出 (阻塞, 带超时)
     * @param timeout_ms 超时毫秒数, 0=无限等待
     * @return true=线程已退出
     */
    bool join(int timeout_ms = 0);

    /**
     * 停止并等待 (等价于 request_stop() + join())
     */
    void stop();

    /**
     * 清理所有资源
     * @note  必须在调用线程 (与 init 同一线程) 中执行
     */
    void cleanup();

    // ========== 状态查询 (线程安全) ==========

    /** 获取当前运行状态 */
    CockpitState state() const;

    /** 是否正在运行 */
    bool is_running() const;

    /** 是否已连接 X-Plane */
    bool is_xplane_connected() const;

    /** 获取飞行状态快照 */
    CockpitFlightState get_flight_state() const;

    /** 获取当前航路点数 */
    int get_route_count() const;

    /** 获取指定航路点 (线程安全) */
    CockpitWaypoint get_waypoint(int index) const;

    /** 获取起飞机场 ICAO */
    std::string get_origin() const;

    /** 获取目的地机场 ICAO */
    std::string get_destination() const;

    /** 获取航班号 */
    std::string get_flight_number() const;

    /** 获取数据帧计数 */
    int get_frame_count() const;

private:
    // ---- 内部实现 ----
    struct Impl;
    Impl* m_impl;

    // ---- 线程管理 ----
    HANDLE              m_thread;       // 后台运行线程句柄
    std::atomic<LONG>   m_running;      // 运行标志 (1=运行)
    std::atomic<int>    m_state;        // CockpitState 枚举值

    // 后台线程入口
    static DWORD WINAPI thread_entry(LPVOID param);
};
