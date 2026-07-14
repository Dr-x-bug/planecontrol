/**
 * cockpit.cpp — Cockpit 子程序可移植实现
 *
 * PIMPL 模式: 所有平台相关代码封装在 CockpitImpl 中,
 * 公开接口仅暴露 cockpit.h 中的纯 C++ 类型。
 */

#include "cockpit_internal.h"

// ============================================================
// CockpitImpl 实现
// ============================================================

CockpitImpl* CockpitImpl::s_active_impl = nullptr;

CockpitImpl::CockpitImpl() {
    mutex_init(&state_lock);
    memset(&cached_state, 0, sizeof(cached_state));
}

CockpitImpl::~CockpitImpl() {
    mutex_del(&state_lock);
}

bool CockpitImpl::nd_init() {
    const std::string& a = asset_path;

    if (!nd_renderer.init()) {
        printf("[Cockpit] ND renderer init failed!\n");
        return false;
    }

    wpts = load_fms((a + "KSEAKBFI.fms").c_str());
    printf("[Cockpit] ND loaded %zu waypoints\n", wpts.size());

    hXpcThread = nd_thread_start();
    if (hXpcThread) {
        thread_sleep(200);
        NDFlightData test;
        if (atomic_read_data(test) || g_shared.frame_count > 0) {
            use_xpc_thread = true;
            printf("[Cockpit] X-Plane data thread ACTIVE\n");
        } else {
            nd_thread_stop(hXpcThread);
            hXpcThread = 0;
            printf("[Cockpit] No X-Plane connection, fallback to file\n");
        }
    }

    if (!use_xpc_thread) {
        if (!nd_data_open((a + "nd.dat").c_str())) {
            printf("[Cockpit] Failed to open nd.dat!\n");
            return false;
        }
        printf("[Cockpit] nd.dat opened: %ld lines (file mode)\n", g_total_lines);
    }

    ht.init();
    load_navaids(ht, (a + "earth_nav.dat").c_str());
    load_fixes(ht,   (a + "earth_fix.dat").c_str());
    load_airports(ht,(a + "apt.dat").c_str());
    printf("[Cockpit] Hash table: %d nav points\n", ht.point_count);

    nd_initialized = true;
    return true;
}

bool CockpitImpl::fmc_init() {
    const std::string& a = asset_path;

    if (!fmc_renderer.init()) {
        printf("[Cockpit] FMC renderer init failed!\n");
        return false;
    }

    load_navdata_avl((a + "earth_nav.dat").c_str(),
                     (a + "earth_fix.dat").c_str(),
                     (a + "apt.dat").c_str());
    load_route_from_fms((a + "KSEAKBFI.fms").c_str());
    printf("[Cockpit] FMC route: %d waypoints\n", g_route.count);

    deparr_data_init();
    init_airport_data();

    fmc_buttons_init();
    fmc_switch_page(PAGE_INDEX);
    printf("[Cockpit] FMC initialized, page: %s\n",
           g_pages[g_screen.current_page].title);

    printf("[Cockpit] Initializing shared memory IPC...\n");
    if (shm_cockpit.create()) sync_route_to_shm();

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

void CockpitImpl::sync_route_to_shm() {
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

void CockpitImpl::sync_to_xplane_fmc(const char* origin, const char* dest,
                                      const char* flt_no) {
    if (!g_xpc_ready || !origin[0] || !dest[0]) return;
    printf("[XPC-FMC] Syncing route to X-Plane FMC: %s -> %s (%s)\n",
           origin, dest, flt_no);
    sendCOMM(g_xpc_sock, "sim/FMS/clear");          thread_sleep(80);
    sendCOMM(g_xpc_sock, "sim/FMS/key_period");     thread_sleep(80);
    sendCOMM(g_xpc_sock, "sim/FMS/clear");          thread_sleep(80);
    sendCOMM(g_xpc_sock, "sim/FMS/fpln");           thread_sleep(100);
    sendCOMM(g_xpc_sock, "sim/FMS/ls_1l");          thread_sleep(80);
    for (const char* p = origin; *p; p++) {
        char cmd[32]; snprintf(cmd, 32, "sim/FMS/key_%c", *p);
        sendCOMM(g_xpc_sock, cmd); thread_sleep(60);
    }
    thread_sleep(100);
    sendCOMM(g_xpc_sock, "sim/FMS/ls_1r");          thread_sleep(80);
    for (const char* p = dest; *p; p++) {
        char cmd[32]; snprintf(cmd, 32, "sim/FMS/key_%c", *p);
        sendCOMM(g_xpc_sock, cmd); thread_sleep(60);
    }
    thread_sleep(100);
    if (flt_no[0]) {
        sendCOMM(g_xpc_sock, "sim/FMS/ls_2l");      thread_sleep(80);
        for (const char* p = flt_no; *p; p++) {
            if ((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'Z')) {
                char cmd[32]; snprintf(cmd, 32, "sim/FMS/key_%c", *p);
                sendCOMM(g_xpc_sock, cmd); thread_sleep(60);
            }
        }
    }
    thread_sleep(100);
    sendCOMM(g_xpc_sock, "sim/FMS/exec");
    printf("[XPC-FMC] Route sync complete\n");
}

void CockpitImpl::read_route_from_shm() {
    SharedRouteData route_data;
    if (shm_cockpit.pData && shm_cockpit.has_new_data(shm_last_version)) {
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

void CockpitImpl::fmc_update_xplane_info() {
    if (!use_xpc_thread || !g_nd_data.pos_valid) return;
    char buf[64];
    snprintf(buf, sizeof(buf), "POS %.4f %.4f", g_nd_data.lat, g_nd_data.lon);
    if (g_screen.current_page == PAGE_PROG)
        g_screen.set_line_L(0, buf);
}

void CockpitImpl::nd_update_data() {
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

void CockpitImpl::nd_render() {
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

void CockpitImpl::fmc_render() {
    fmc_renderer.clear();
    fmc_renderer.draw_bg();

    for (int i = 0; i < FMC_KEY_COUNT; i++) {
        FMCButton& btn = fmc_buttons[i];
        if (!btn.label) continue;
        if (btn.shape == FMC_SHAPE_CIRCLE)
            fmc_renderer.draw_btn_circle(btn.rect.x + btn.rect.w / 2,
                btn.rect.y + btn.rect.h / 2, btn.radius, btn.state);
        else
            fmc_renderer.draw_btn_rect(btn.rect.x, btn.rect.y,
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

void CockpitImpl::update_cached_state() {
    mutex_lock(&state_lock);
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
    mutex_unlock(&state_lock);
}

bool CockpitImpl::handle_events(bool& running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { running = false; return false; }
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
            { running = false; return false; }

        if (e.type == SDL_WINDOWEVENT &&
            e.window.event == SDL_WINDOWEVENT_CLOSE &&
            e.window.windowID == SDL_GetWindowID(nd_renderer.window))
            { running = false; return false; }

        if (e.type == SDL_WINDOWEVENT &&
            e.window.event == SDL_WINDOWEVENT_RESIZED &&
            e.window.windowID == SDL_GetWindowID(fmc_renderer.window)) {
            int w, h;
            SDL_GetWindowSize(fmc_renderer.window, &w, &h);
            fmc_renderer.update_scale(w, h);
        }

        if (e.type == SDL_MOUSEMOTION) {
            int w, h;
            SDL_GetWindowSize(fmc_renderer.window, &w, &h);
            int idx = fmc_hit_test(e.motion.x, e.motion.y,
                                   fmc_renderer.scale, w, h);
            static int hover_idx = -1;
            if (idx != hover_idx) { fmc_update_hover(idx); hover_idx = idx; }
        }

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

        if (e.type == SDL_TEXTINPUT && e.text.text[0] >= 32)
            g_screen.type_char(e.text.text[0]);
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_BACKSPACE)
            g_screen.backspace();
    }
    return running;
}

void CockpitImpl::main_loop_frame() {
    nd_update_data();
    fmc_update_xplane_info();

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
    SDL_Delay(16);
}

void CockpitImpl::cleanup_all() {
    if (use_xpc_thread && hXpcThread) {
        nd_thread_stop(hXpcThread);
        hXpcThread = 0;
    }
    nd_data_close();
    shm_cockpit.close();
    ht.destroy();
    destroy_airport_data();

    if (s_active_impl == this) {
        s_active_impl = nullptr;
        g_route_sync_cb = nullptr;
        g_xplane_fmc_sync_cb = nullptr;
    }

    printf("\n[Cockpit] Shutdown complete.\n");
}

// ============================================================
// 平台相关线程入口 (Windows 专用)
// ============================================================
#ifdef _WIN32
static DWORD WINAPI cockpit_thread_entry(LPVOID param) {
    CockpitContext* ctx = static_cast<CockpitContext*>(param);
    printf("[Cockpit] Background thread started (ID=%lu)\n", GetCurrentThreadId());
    ctx->run();
    printf("[Cockpit] Background thread exited (ID=%lu)\n", GetCurrentThreadId());
    return 0;
}
#endif

// ============================================================
// CockpitContext 公开接口
// ============================================================

CockpitContext::CockpitContext() : m_impl(new CockpitImpl()) {}
CockpitContext::~CockpitContext() { delete m_impl; m_impl = nullptr; }

bool CockpitContext::init() {
    if (m_impl->m_state.load() != 0) {
        printf("[Cockpit] Already initialized!\n");
        return false;
    }

    printf("========================================\n");
    printf("  Cockpit — ND + FMC + X-Plane Connect\n");
    printf("========================================\n\n");

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("[Cockpit] SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    TTF_Init();
    IMG_Init(IMG_INIT_PNG);

    if (!m_impl->nd_init())   { SDL_Quit(); return false; }
    if (!m_impl->fmc_init())  { m_impl->ht.destroy(); SDL_Quit(); return false; }

    int w, h;
    SDL_GetWindowSize(m_impl->fmc_renderer.window, &w, &h);
    m_impl->fmc_renderer.update_scale(w, h);

    printf("\n[Cockpit] All modules initialized. Ready to run.\n");
    printf("[Cockpit] ND window + FMC window — Press ESC or close any window to exit\n\n");

    m_impl->m_state.store(1);  // READY
    return true;
}

void CockpitContext::run() {
    if (m_impl->m_state.load() != 1) {
        printf("[Cockpit] Not ready! Call init() first.\n");
        return;
    }
    m_impl->m_running.store(true);
    m_impl->m_state.store(2);  // RUNNING

    bool running = true;
    while (running && m_impl->m_running.load()) {
        if (!m_impl->handle_events(running)) break;
        if (!running || !m_impl->m_running.load()) break;
        m_impl->main_loop_frame();
    }

    m_impl->m_state.store(4);  // STOPPED
    m_impl->m_running.store(false);
}

bool CockpitContext::poll_frame() {
    if (m_impl->m_state.load() != 2) {
        // 如果尚未运行，先切换到运行状态
        if (m_impl->m_state.load() == 1) m_impl->m_state.store(2);
    }
    bool running = true;
    bool ok = m_impl->handle_events(running);
    if (!ok || !running) return false;
    m_impl->main_loop_frame();
    return m_impl->m_running.load();
}

bool CockpitContext::start() {
    if (m_impl->m_state.load() != 1) {
        printf("[Cockpit] Not ready! Call init() first.\n");
        return false;
    }
    if (m_impl->m_thread) {
        printf("[Cockpit] Already started!\n");
        return false;
    }

    m_impl->m_running.store(true);

#ifdef _WIN32
    m_impl->m_thread = CreateThread(NULL, 0, cockpit_thread_entry, this, 0, NULL);
    if (!m_impl->m_thread) {
        printf("[Cockpit] CreateThread failed! Error: %lu\n", GetLastError());
        m_impl->m_running.store(false);
        return false;
    }
    for (int i = 0; i < 50; i++) {
        if (m_impl->m_state.load() == 2) return true;
        Sleep(10);
    }
#else
    // POSIX 线程 (未完整实现, 预留接口)
    // pthread_create(&m_impl->m_thread, NULL, cockpit_thread_entry, this);
#endif

    printf("[Cockpit] Thread started (may be initializing)\n");
    return true;
}

void CockpitContext::request_stop() {
    m_impl->m_running.store(false);
    m_impl->m_state.store(3);  // STOPPING
    SDL_Event quit_event;
    quit_event.type = SDL_QUIT;
    SDL_PushEvent(&quit_event);
}

bool CockpitContext::join(int timeout_ms) {
#ifdef _WIN32
    if (!m_impl->m_thread) return true;
    DWORD ms = (timeout_ms <= 0) ? INFINITE : (DWORD)timeout_ms;
    DWORD r = WaitForSingleObject(m_impl->m_thread, ms);
    if (r == WAIT_OBJECT_0) { CloseHandle(m_impl->m_thread); m_impl->m_thread = 0; return true; }
    return false;
#else
    return true;  // POSIX 平台暂未实现 wait
#endif
}

void CockpitContext::stop() {
    request_stop();
    join(5000);
}

void CockpitContext::cleanup() {
    if (m_impl->m_state.load() == 2) stop();
    m_impl->cleanup_all();
    // SDL/TTF/IMG quit 由主程序统一管理, 这里不调用
    m_impl->m_state.store(0);  // IDLE
}

CockpitState CockpitContext::state() const {
    return static_cast<CockpitState>(m_impl->m_state.load());
}
bool CockpitContext::is_running()          const { return m_impl->m_state.load() == 2; }
bool CockpitContext::is_xplane_connected() const {
    mutex_lock(&m_impl->state_lock);
    bool v = m_impl->cached_state.xplane_connected;
    mutex_unlock(&m_impl->state_lock);
    return v;
}

CockpitFlightState CockpitContext::get_flight_state() const {
    mutex_lock(&m_impl->state_lock);
    CockpitFlightState s = m_impl->cached_state;
    mutex_unlock(&m_impl->state_lock);
    return s;
}

int CockpitContext::get_route_count() const { return g_route.count; }

CockpitWaypoint CockpitContext::get_waypoint(int idx) const {
    CockpitWaypoint w;
    if (idx >= 0 && idx < g_route.count) {
        const RouteWpt& rw = g_route.wpts[idx];
        strncpy(w.id, rw.id, 15); w.id[15] = '\0';
        w.lat = rw.lat; w.lon = rw.lon;
        w.freq = rw.freq; w.type = rw.type;
    }
    return w;
}

std::string CockpitContext::get_origin()        const { return g_screen.origin; }
std::string CockpitContext::get_destination()   const { return g_screen.dest; }
std::string CockpitContext::get_flight_number() const { return g_screen.flt_no; }
int CockpitContext::get_frame_count()           const { return g_shared.frame_count; }

void CockpitContext::set_asset_path(const std::string& path) {
    m_impl->asset_path = path;
    // 确保以 / 结尾
    if (!m_impl->asset_path.empty() && m_impl->asset_path.back() != '/' &&
        m_impl->asset_path.back() != '\\')
        m_impl->asset_path += '/';
}

const char* CockpitContext::version() {
    return "Cockpit v2.0 (ND+FMC+X-Plane)";
}
