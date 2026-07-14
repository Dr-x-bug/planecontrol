/**
 * cockpit_internal.h — Cockpit 子程序内部实现头文件
 *
 * 此文件包含所有平台相关头文件和ND/FMC模块头文件。
 * 外部程序不应直接引用此文件 — 请使用 cockpit.h
 */

#pragma once

// ============================================================
// 平台适配层
// ============================================================
#ifdef _WIN32
  #include <winsock2.h>   // 必须在 windows.h 之前 (X-Plane Connect)
  #include <windows.h>
  #define COCKPIT_THREAD  HANDLE
  #define COCKPIT_MUTEX   CRITICAL_SECTION
  #define mutex_init(m)   InitializeCriticalSection(m)
  #define mutex_del(m)    DeleteCriticalSection(m)
  #define mutex_lock(m)   EnterCriticalSection(m)
  #define mutex_unlock(m) LeaveCriticalSection(m)
  #define thread_sleep(ms) Sleep(ms)
  using atomic_long = LONG;
  inline void atomic_store(volatile LONG* p, LONG v) { InterlockedExchange(p, v); }
  inline LONG atomic_load(volatile LONG* p) { return InterlockedCompareExchange(p, 0, 0); }
#else
  #include <pthread.h>
  #include <unistd.h>
  #define COCKPIT_THREAD  pthread_t
  #define COCKPIT_MUTEX   pthread_mutex_t
  #define mutex_init(m)   pthread_mutex_init(m, nullptr)
  #define mutex_del(m)    pthread_mutex_destroy(m)
  #define mutex_lock(m)   pthread_mutex_lock(m)
  #define mutex_unlock(m) pthread_mutex_unlock(m)
  #define thread_sleep(ms) usleep((ms) * 1000)
  using atomic_long = long;
  // 跨平台原子操作: 用 GCC/Clang 内建函数
  inline void atomic_store(volatile long* p, long v) { __sync_lock_test_and_set(p, v); }
  inline long atomic_load(volatile long* p) { return __sync_fetch_and_add(p, 0); }
#endif

// ============================================================
// 标准库
// ============================================================
#include <cstdio>
#include <cstring>
#include <cmath>
#include <atomic>
#include <string>
#include <vector>

// ============================================================
// SDL 图形库
// ============================================================
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

// ============================================================
// ND 模块
// ============================================================
#include "ND/config.h"
#include "ND/renderer.h"
#include "ND/navdata.h"
#include "ND/nd_data.h"
#include "ND/nd_xpc.h"
#include "ND/nd_map.h"
#include "ND/navaid_hash.h"
#include "ND/nd_thread.h"

// ============================================================
// FMC 模块
// ============================================================
#include "FMC/config.h"
#include "FMC/renderer.h"
#include "FMC/fmc_ui.h"
#include "FMC/fmc_pages.h"
#include "FMC/fmc_route.h"
#include "FMC/fmc_deparr.h"
#include "FMC/fmc_data.h"

// ============================================================
// 共享内存 IPC
// ============================================================
#include "shared_mem.h"
#include "fmc_shm_sync.h"

// ============================================================
// CockpitImpl 声明 (内部实现)
// ============================================================
#include "cockpit.h"

struct CockpitImpl {
    // ---- ND 模块 ----
    NDRenderer               nd_renderer;
    std::vector<Waypoint>    wpts;
    GridHashTable            ht;
    bool                     nd_initialized = false;

    // ---- FMC 模块 ----
    FMCRenderer              fmc_renderer;
    bool                     fmc_initialized = false;

    // ---- X-Plane 连接 ----
    COCKPIT_THREAD           hXpcThread = 0;
    bool                     use_xpc_thread = false;

    // ---- 共享内存 IPC ----
    SharedMemoryIPC          shm_cockpit;
    LONG                     shm_last_version = 0;

    // ---- 文件模式/时间 ----
    Uint32                   last_file_update = 0;
    Uint32                   last_shm_sync   = 0;

    // ---- 资产路径 ----
    std::string              asset_path;

    // ---- 同步回调 (静态单例, 供全局回调使用) ----
    static CockpitImpl*      s_active_impl;

    // ---- 线程安全状态 ----
    mutable COCKPIT_MUTEX    state_lock;
    CockpitFlightState       cached_state;

    // ---- 后台线程 ----
    COCKPIT_THREAD           m_thread = 0;
    std::atomic<bool>        m_running{false};
    std::atomic<int>         m_state{0};  // CockpitState

    CockpitImpl();
    ~CockpitImpl();

    // 模块初始化
    bool nd_init();
    bool fmc_init();

    // 同步
    void sync_route_to_shm();
    void sync_to_xplane_fmc(const char* origin, const char* dest, const char* flt_no);
    void read_route_from_shm();

    // 更新/渲染
    void fmc_update_xplane_info();
    void nd_update_data();
    void nd_render();
    void fmc_render();
    void update_cached_state();
    bool handle_events(bool& running);
    void main_loop_frame();
    void cleanup_all();
};
