/**
 * cockpit_api.cpp — CockpitContext 实现
 *
 * 将原 cockpit_main.cpp 中的初始化、主循环、清理逻辑封装为
 * CockpitContext 类的方法, 支持多线程调用。
 */

#include <winsock2.h>   // 必须在 windows.h 之前 (X-Plane Connect 需要)
#include <windows.h>
#include "cockpit_api.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <new>

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

// ============================================================
// 共享内存 IPC
// ============================================================
#include "shared_mem.h"
#include "fmc_shm_sync.h"

// ============================================================
// 内部实现结构 (PIMPL idiom — 隐藏所有实现细节)
// ============================================================
struct CockpitContext::Impl {
    // ---- ND 模块状态 ----
    NDRenderer          nd_renderer;
    std::vector<Waypoint> wpts;
    GridHashTable       ht;
    bool                nd_initialized = false;

    // ---- FMC 模块状态 ----
    FMCRenderer         fmc_renderer;
    bool                fmc_initialized = false;

    // ---- X-Plane 连接 ----
    HANDLE              hXpcThread = NULL;
    bool                use_xpc_thread = false;

    // ---- 共享内存 IPC ----
    SharedMemoryIPC     shm_cockpit;
    LONG                shm_last_version = 0;

    // ---- 文件模式数据 ----
    Uint32              last_file_update = 0;
    Uint32              last_shm_sync = 0;

    // ---- 同步回调 (设为静态以兼容全局回调) ----
    // 注意: 这些回调指向 Impl 实例的方法, 通过捕获的 this 指针调用
    static Impl* s_active_impl;  // 当前活动实例 (单例模式, 供全局回调使用)

    // ---- 线程安全: 飞行状态快照 ----
    mutable CRITICAL_SECTION  state_lock;
    CockpitFlightState        cached_state;

    Impl()  { InitializeCriticalSection(&state_lock); }
    ~Impl() { DeleteCriticalSection(&state_lock); }

    // ========== ND 模块初始化 ==========
    bool nd_init() {
        if (!nd_renderer.init()) {
            printf("[Cockpit] ND renderer init failed!\n");
            return false;
        }

        // 加载飞行计划
        wpts = load_fms("assets/KSEAKBFI.fms");
        printf("[Cockpit] ND loaded %zu waypoints\n", wpts.size());

        // 启动 X-Plane 数据线程
        hXpcThread = nd_thread_start();
        if (hXpcThread) {
            Sleep(200);
            NDFlightData test;
            if (atomic_read_data(test) || g_shared.frame_count > 0) {
                use_xpc_thread = true;
                printf("[Cockpit] X-Plane data thread ACTIVE\n");
            } else {
                nd_thread_stop(hXpcThread);
                hXpcThread = NULL;
                printf("[Cockpit] No X-Plane connection, fallback to file\n");
            }
        }

        // 回退: 文件数据模式
        if (!use_xpc_thread) {
            if (!nd_data_open("assets/nd.dat")) {
                printf("[Cockpit] Failed to open nd.dat!\n");
                return false;
            }
            printf("[Cockpit] nd.dat opened: %ld lines (file mode)\n", g_total_lines);
        }

        // 加载导航数据库
        ht.init();
        load_navaids(ht, "assets/earth_nav.dat");
        load_fixes(ht, "assets/earth_fix.dat");
        load_airports(ht, "assets/apt.dat");
        printf("[Cockpit] Hash table: %d nav points\n", ht.point_count);

        nd_initialized = true;
        return true;
    }

    // ========== FMC 模块初始化 ==========
    bool fmc_init() {
        if (!fmc_renderer.init()) {
            printf("[Cockpit] FMC renderer init failed!\n");
            return false;
        }

        // 加载导航数据到 AVL 树 + 飞行计划
        load_navdata_avl("assets/earth_nav.dat",
                         "assets/earth_fix.dat",
                         "assets/apt.dat");
        load_route_from_fms("assets/KSEAKBFI.fms");
        printf("[Cockpit] FMC route: %d waypoints\n", g_route.count);

        // 初始化按钮
        fmc_buttons_init();
        fmc_switch_page(PAGE_INDEX);
        printf("[Cockpit] FMC initialized, page: %s\n",
               g_pages[g_screen.current_page].title);

        // 初始化共享内存 (FMC作为创建方)
        printf("[Cockpit] Initializing shared memory IPC...\n");
        if (shm_cockpit.create()) {
            sync_route_to_shm();
        }

        // 注册全局同步回调
        s_active_impl = this;
        g_route_sync_cb = []() {
            if (s_active_impl) s_active_impl->sync_route_to_shm();
        };
        g_xplane_fmc_sync_cb = [](const char* o, const char* d, const char* f) {
            if (s_active_impl) s_active_impl->sync_to_xplane_fmc(o, d, f);
        };

        fmc_initialized = true;
        return true;
    }

    // ========== 航路 → 共享内存同步 ==========
    void sync_route_to_shm() {
        if (!shm_cockpit.pData) return;

        SharedRoutePoint shm_wpts[SHM_MAX_WPTS];
        int count = g_route.count;
        if (count > SHM_MAX_WPTS) count = SHM_MAX_WPTS;

        for (int i = 0; i < count; i++) {
            strncpy(shm_wpts[i].id, g_route.wpts[i].id, SHM_NAME_MAX - 1);
            shm_wpts[i].id[SHM_NAME_MAX - 1] = '\0';
            shm_wpts[i].lat  = g_route.wpts[i].lat;
            shm_wpts[i].lon  = g_route.wpts[i].lon;
            shm_wpts[i].freq = g_route.wpts[i].freq;
            shm_wpts[i].type = g_route.wpts[i].type;
        }

        shm_cockpit.write_route(shm_wpts, count,
            g_screen.origin, g_screen.dest, g_screen.flt_no);
    }

    // ========== FMC→X-Plane 航路同步 ==========
    void sync_to_xplane_fmc(const char* origin, const char* dest,
                            const char* flt_no) {
        if (!g_xpc_ready) return;
        if (!origin[0] || !dest[0]) return;

        printf("[XPC-FMC] Syncing route to X-Plane FMC: %s -> %s (%s)\n",
               origin, dest, flt_no);

        sendCOMM(g_xpc_sock, "sim/FMS/clear");
        Sleep(80);
        sendCOMM(g_xpc_sock, "sim/FMS/key_period");
        Sleep(80);
        sendCOMM(g_xpc_sock, "sim/FMS/clear");
        Sleep(80);

        sendCOMM(g_xpc_sock, "sim/FMS/fpln");
        Sleep(100);

        sendCOMM(g_xpc_sock, "sim/FMS/ls_1l");
        Sleep(80);
        for (const char* p = origin; *p; p++) {
            char cmd[32];
            snprintf(cmd, 32, "sim/FMS/key_%c", *p);
            sendCOMM(g_xpc_sock, cmd);
            Sleep(60);
        }
        Sleep(100);

        sendCOMM(g_xpc_sock, "sim/FMS/ls_1r");
        Sleep(80);
        for (const char* p = dest; *p; p++) {
            char cmd[32];
            snprintf(cmd, 32, "sim/FMS/key_%c", *p);
            sendCOMM(g_xpc_sock, cmd);
            Sleep(60);
        }
        Sleep(100);

        if (flt_no[0]) {
            sendCOMM(g_xpc_sock, "sim/FMS/ls_2l");
            Sleep(80);
            for (const char* p = flt_no; *p; p++) {
                char cmd[32];
                if ((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'Z')) {
                    snprintf(cmd, 32, "sim/FMS/key_%c", *p);
                    sendCOMM(g_xpc_sock, cmd);
                    Sleep(60);
                }
            }
        }

        Sleep(100);
        sendCOMM(g_xpc_sock, "sim/FMS/exec");
        printf("[XPC-FMC] Route sync complete\n");
    }

    // ========== 共享内存 → ND 航路读取 ==========
    void read_route_from_shm() {
        SharedRouteData route_data;
        if (shm_cockpit.pData &&
            shm_cockpit.has_new_data(shm_last_version)) {
            if (shm_cockpit.read_route(route_data) && route_data.wpt_count > 0) {
                wpts.clear();
                for (int i = 0; i < route_data.wpt_count; i++) {
                    Waypoint wp;
                    wp.id  = route_data.wpts[i].id;
                    wp.lat = route_data.wpts[i].lat;
                    wp.lon = route_data.wpts[i].lon;
                    wpts.push_back(wp);
                }
                printf("[Cockpit] ND synced %d waypoints from FMC (shm ver=%ld)\n",
                       route_data.wpt_count, route_data.version);
            }
        }
    }

    // ========== FMC 更新 X-Plane 位置信息 ==========
    void fmc_update_xplane_info() {
        if (!use_xpc_thread || !g_nd_data.pos_valid) return;

        char buf[64];
        snprintf(buf, sizeof(buf), "POS %.4f %.4f", g_nd_data.lat, g_nd_data.lon);
        if (g_screen.current_page == PAGE_PROG) {
            g_screen.set_line_L(0, buf);
        }
    }

    // ========== ND 数据更新 ==========
    void nd_update_data() {
        if (use_xpc_thread) {
            NDFlightData latest;
            if (atomic_read_data(latest)) {
                g_nd.ac_lat   = latest.lat;
                g_nd.ac_lon   = latest.lon;
                g_nd.heading  = latest.heading;
                g_nd.gs       = latest.gs;
                g_nd.tas      = latest.tas;
                g_nd.wind_dir = latest.wind_dir;
                g_nd.wind_spd = latest.wind_spd;
            }
        } else {
            Uint32 now = SDL_GetTicks();
            if (now - last_file_update >= 100) {
                last_file_update = now;
                nd_data_read_next();
                printf("\r[Cockpit] Frame:%ld/%ld  HDG=%.0f  GS=%.0fkt  ",
                       g_current_line, g_total_lines, g_nd.heading, g_nd.gs);
                fflush(stdout);
            }
        }
    }

    // ========== ND 渲染 ==========
    void nd_render() {
        nd_renderer.clear();
        draw_nd_map(nd_renderer, wpts, ht);

        char buf[64];
        if (use_xpc_thread) {
            snprintf(buf, sizeof(buf), "X-PLANE #%ld  GS:%.0fkt",
                     g_shared.frame_count, g_nd.gs);
            nd_renderer.draw_text(ND_WIN_W - 200, ND_WIN_H - 42, buf,
                                  Color::GREEN_LT, true);
        } else {
            snprintf(buf, sizeof(buf), "FILE:%ld/%ld  GS:%.0fkt",
                     g_current_line, g_total_lines, g_nd.gs);
            nd_renderer.draw_text(ND_WIN_W - 200, ND_WIN_H - 42, buf,
                                  Color::GRAY, true);
        }

        if (use_xpc_thread) {
            const char* status = g_nd_data.connected ? "X-PLANE ONLINE" : "X-PLANE WAIT";
            SDL_Color c = g_nd_data.connected ? Color::GREEN_LT : Color::YELLOW;
            nd_renderer.draw_text(10, ND_WIN_H - 42, status, c, true);
        }

        nd_renderer.present();
    }

    // ========== FMC 渲染 ==========
    void fmc_render() {
        fmc_renderer.clear();
        fmc_renderer.draw_bg();

        // 按键高亮
        for (int i = 0; i < FMC_KEY_COUNT; i++) {
            FMCButton& btn = fmc_buttons[i];
            if (!btn.label) continue;
            if (btn.shape == FMC_SHAPE_CIRCLE)
                fmc_renderer.draw_btn_circle(
                    btn.rect.x + btn.rect.w / 2,
                    btn.rect.y + btn.rect.h / 2,
                    btn.radius, btn.state);
            else
                fmc_renderer.draw_btn_rect(
                    btn.rect.x, btn.rect.y,
                    btn.rect.w, btn.rect.h, btn.state);
        }

        fmc_draw_screen(fmc_renderer);

        if (use_xpc_thread && g_nd_data.pos_valid) {
            char buf[64];
            snprintf(buf, sizeof(buf), "X-PLANE  HDG:%.0f  GS:%.0fkt  ALT:%.0fft",
                     g_nd_data.heading, g_nd_data.gs, g_nd_data.alt);
            fmc_renderer.draw_text(10, 960, buf, Color::FMC_CYAN, true);
        } else {
            char buf[48];
            snprintf(buf, sizeof(buf), "Page: %s  [NO X-PLANE]",
                     g_pages[g_screen.current_page].title);
            fmc_renderer.draw_text(10, 960, buf, Color::FMC_GREEN, true);
        }

        fmc_renderer.present();
    }

    // ========== 更新缓存状态 (线程安全) ==========
    void update_cached_state() {
        EnterCriticalSection(&state_lock);
        cached_state.lat       = g_nd_data.lat;
        cached_state.lon       = g_nd_data.lon;
        cached_state.heading   = g_nd_data.heading;
        cached_state.gs        = g_nd_data.gs;
        cached_state.tas       = g_nd_data.tas;
        cached_state.alt       = g_nd_data.alt;
        cached_state.wind_dir  = g_nd_data.wind_dir;
        cached_state.wind_spd  = g_nd_data.wind_spd;
        cached_state.xplane_connected = (use_xpc_thread && g_nd_data.connected);
        cached_state.pos_valid = g_nd_data.pos_valid;
        cached_state.frame_count = g_shared.frame_count;
        LeaveCriticalSection(&state_lock);
    }

    // ========== 主循环事件处理 ==========
    // 返回 false 表示需要退出
    bool handle_events(bool& running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            // 全局退出
            if (e.type == SDL_QUIT) { running = false; return false; }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
                { running = false; return false; }

            // ND 窗口关闭
            if (e.type == SDL_WINDOWEVENT &&
                e.window.event == SDL_WINDOWEVENT_CLOSE &&
                e.window.windowID == SDL_GetWindowID(nd_renderer.window))
                { running = false; return false; }

            // FMC 窗口事件
            if (e.type == SDL_WINDOWEVENT &&
                e.window.event == SDL_WINDOWEVENT_RESIZED &&
                e.window.windowID == SDL_GetWindowID(fmc_renderer.window)) {
                int w, h;
                SDL_GetWindowSize(fmc_renderer.window, &w, &h);
                fmc_renderer.update_scale(w, h);
            }

            // FMC 鼠标悬停
            if (e.type == SDL_MOUSEMOTION) {
                int w, h;
                SDL_GetWindowSize(fmc_renderer.window, &w, &h);
                int idx = fmc_hit_test(e.motion.x, e.motion.y,
                                       fmc_renderer.scale, w, h);
                static int hover_idx = -1;
                if (idx != hover_idx) {
                    fmc_update_hover(idx);
                    hover_idx = idx;
                }
            }

            // FMC 鼠标点击
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                int w, h;
                SDL_GetWindowSize(fmc_renderer.window, &w, &h);
                int idx = fmc_hit_test(e.button.x, e.button.y,
                                       fmc_renderer.scale, w, h);
                if (idx >= 0) {
                    printf("[FMC] Click: %s (page: %s)\n",
                           fmc_buttons[idx].label,
                           g_pages[g_screen.current_page].title);
                    fmc_handle_click(idx);
                }
            }

            // FMC 键盘输入
            if (e.type == SDL_TEXTINPUT && e.text.text[0] >= 32)
                g_screen.type_char(e.text.text[0]);
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_BACKSPACE)
                g_screen.backspace();
        }
        return running;
    }

    // ========== 主循环单帧 ==========
    void main_loop_frame() {
        nd_update_data();
        fmc_update_xplane_info();

        // 定期检查共享内存 (每100ms)
        Uint32 now_shm = SDL_GetTicks();
        if (now_shm - last_shm_sync >= 100) {
            last_shm_sync = now_shm;
            read_route_from_shm();
            if (use_xpc_thread && g_nd_data.pos_valid) {
                shm_cockpit.update_aircraft_position(
                    g_nd_data.lat, g_nd_data.lon, g_nd_data.heading);
            }
        }

        nd_render();
        fmc_render();
        update_cached_state();

        SDL_Delay(16);  // ~60fps
    }

    // ========== 清理 ==========
    void cleanup_all() {
        // 停止 XPC 数据线程
        if (use_xpc_thread && hXpcThread) {
            nd_thread_stop(hXpcThread);
            hXpcThread = NULL;
        }
        nd_data_close();
        shm_cockpit.close();
        ht.destroy();

        // 清除全局回调 (避免悬空指针)
        if (s_active_impl == this) {
            s_active_impl = nullptr;
            g_route_sync_cb = nullptr;
            g_xplane_fmc_sync_cb = nullptr;
        }

        printf("\n[Cockpit] Shutdown complete.\n");
    }
};

// 静态成员初始化
CockpitContext::Impl* CockpitContext::Impl::s_active_impl = nullptr;

// ============================================================
// CockpitContext 公开接口实现
// ============================================================

CockpitContext::CockpitContext()
    : m_impl(new Impl())
    , m_thread(NULL)
    , m_running(0)
    , m_state(static_cast<int>(CockpitState::IDLE))
{
}

CockpitContext::~CockpitContext() {
    if (is_running()) {
        stop();
    }
    if (m_state.load() == static_cast<int>(CockpitState::READY) ||
        m_state.load() == static_cast<int>(CockpitState::STOPPED)) {
        cleanup();
    }
    delete m_impl;
    m_impl = nullptr;
}

bool CockpitContext::init() {
    if (m_state.load() != static_cast<int>(CockpitState::IDLE)) {
        printf("[Cockpit] Already initialized!\n");
        return false;
    }

    printf("========================================\n");
    printf("  Cockpit — ND + FMC + X-Plane Connect\n");
    printf("========================================\n\n");

    // ---- 全局 SDL 初始化 ----
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("[Cockpit] SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    TTF_Init();
    IMG_Init(IMG_INIT_PNG);

    // ---- ND 模块 ----
    if (!m_impl->nd_init()) {
        printf("[Cockpit] ND init failed!\n");
        SDL_Quit();
        return false;
    }

    // ---- FMC 模块 ----
    if (!m_impl->fmc_init()) {
        printf("[Cockpit] FMC init failed!\n");
        m_impl->ht.destroy();
        SDL_Quit();
        return false;
    }

    // 初始化 FMC 缩放
    {
        int w, h;
        SDL_GetWindowSize(m_impl->fmc_renderer.window, &w, &h);
        m_impl->fmc_renderer.update_scale(w, h);
    }

    printf("\n[Cockpit] All modules initialized. Ready to run.\n");
    printf("[Cockpit] ND window + FMC window — Press ESC or close any window to exit\n\n");

    m_state.store(static_cast<int>(CockpitState::READY));
    return true;
}

void CockpitContext::run() {
    if (m_state.load() != static_cast<int>(CockpitState::READY)) {
        printf("[Cockpit] Not ready to run! Call init() first.\n");
        return;
    }

    m_running.store(1);
    m_state.store(static_cast<int>(CockpitState::RUNNING));

    bool running = true;
    while (running && m_running.load() == 1) {
        if (!m_impl->handle_events(running))
            break;
        if (!running || m_running.load() != 1)
            break;
        m_impl->main_loop_frame();
    }

    m_state.store(static_cast<int>(CockpitState::STOPPED));
    m_running.store(0);
}

// 后台线程入口
DWORD WINAPI CockpitContext::thread_entry(LPVOID param) {
    CockpitContext* ctx = static_cast<CockpitContext*>(param);
    printf("[Cockpit] Background thread started (ID=%lu)\n", GetCurrentThreadId());
    ctx->run();
    printf("[Cockpit] Background thread exited (ID=%lu)\n", GetCurrentThreadId());
    return 0;
}

bool CockpitContext::start() {
    if (m_state.load() != static_cast<int>(CockpitState::READY)) {
        printf("[Cockpit] Not ready to start! Call init() first.\n");
        return false;
    }

    if (m_thread) {
        printf("[Cockpit] Already started!\n");
        return false;
    }

    m_running.store(1);

    m_thread = CreateThread(
        NULL,               // 默认安全属性
        0,                  // 默认栈大小
        thread_entry,       // 线程函数
        this,               // 参数: CockpitContext 指针
        0,                  // 立即运行
        NULL                // 不需要线程ID
    );

    if (!m_thread) {
        printf("[Cockpit] Failed to create background thread! Error: %lu\n",
               GetLastError());
        m_running.store(0);
        return false;
    }

    // 等待线程进入运行状态 (最多等500ms)
    for (int i = 0; i < 50; i++) {
        if (m_state.load() == static_cast<int>(CockpitState::RUNNING))
            return true;
        Sleep(10);
    }

    // 超时但仍返回true (线程可能正在初始化)
    printf("[Cockpit] Thread started but state not yet RUNNING (may be initializing)\n");
    return true;
}

void CockpitContext::request_stop() {
    m_running.store(0);
    m_state.store(static_cast<int>(CockpitState::STOPPING));

    // 发送 SDL_QUIT 事件以唤醒可能阻塞在 PollEvent 的线程
    SDL_Event quit_event;
    quit_event.type = SDL_QUIT;
    SDL_PushEvent(&quit_event);
}

bool CockpitContext::join(int timeout_ms) {
    if (!m_thread) return true;

    DWORD wait_ms = (timeout_ms <= 0) ? INFINITE : (DWORD)timeout_ms;
    DWORD result = WaitForSingleObject(m_thread, wait_ms);

    if (result == WAIT_OBJECT_0) {
        CloseHandle(m_thread);
        m_thread = NULL;
        m_state.store(static_cast<int>(CockpitState::STOPPED));
        printf("[Cockpit] Background thread joined successfully\n");
        return true;
    } else if (result == WAIT_TIMEOUT) {
        printf("[Cockpit] Thread join timeout (%d ms)\n", timeout_ms);
        return false;
    }

    printf("[Cockpit] Thread join error: %lu\n", GetLastError());
    return false;
}

void CockpitContext::stop() {
    request_stop();
    join(5000);  // 等待最多5秒

    // 如果还没退出, 强制终止
    if (m_thread) {
        printf("[Cockpit] Force terminating background thread\n");
        TerminateThread(m_thread, 0);
        CloseHandle(m_thread);
        m_thread = NULL;
        m_state.store(static_cast<int>(CockpitState::STOPPED));
    }
}

void CockpitContext::cleanup() {
    m_impl->cleanup_all();
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    m_state.store(static_cast<int>(CockpitState::IDLE));
}

// ============================================================
// 状态查询 (线程安全)
// ============================================================

CockpitState CockpitContext::state() const {
    return static_cast<CockpitState>(m_state.load());
}

bool CockpitContext::is_running() const {
    return m_state.load() == static_cast<int>(CockpitState::RUNNING);
}

bool CockpitContext::is_xplane_connected() const {
    EnterCriticalSection(&m_impl->state_lock);
    bool connected = m_impl->cached_state.xplane_connected;
    LeaveCriticalSection(&m_impl->state_lock);
    return connected;
}

CockpitFlightState CockpitContext::get_flight_state() const {
    EnterCriticalSection(&m_impl->state_lock);
    CockpitFlightState state = m_impl->cached_state;
    LeaveCriticalSection(&m_impl->state_lock);
    return state;
}

int CockpitContext::get_route_count() const {
    return g_route.count;
}

CockpitWaypoint CockpitContext::get_waypoint(int index) const {
    CockpitWaypoint wp;
    memset(&wp, 0, sizeof(wp));
    if (index >= 0 && index < g_route.count) {
        strncpy(wp.id, g_route.wpts[index].id, 15);
        wp.id[15] = '\0';
        wp.lat  = g_route.wpts[index].lat;
        wp.lon  = g_route.wpts[index].lon;
        wp.freq = g_route.wpts[index].freq;
        wp.type = g_route.wpts[index].type;
    }
    return wp;
}

std::string CockpitContext::get_origin() const {
    return std::string(g_screen.origin);
}

std::string CockpitContext::get_destination() const {
    return std::string(g_screen.dest);
}

std::string CockpitContext::get_flight_number() const {
    return std::string(g_screen.flt_no);
}

int CockpitContext::get_frame_count() const {
    return g_shared.frame_count;
}
