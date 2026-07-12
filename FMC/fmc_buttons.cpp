#include "fmc_ui.h"
#include "fmc_pages.h"
#include "fmc_route.h"
#include "fmc_deparr.h"
#include "../fmc_shm_sync.h"
#include <cstring>

FMCButton fmc_buttons[FMC_KEY_COUNT];
char fmc_lines_L[6][24] = {"", "", "", "", "", ""};
char fmc_lines_R[6][24] = {"", "", "", "", "", ""};
char fmc_title[32] = "INIT/REF INDEX";
char fmc_scratchpad[32] = "";
bool fmc_exec_light = false;

// ---- 函数键 → 页面映射 ----
static FMCPage func_to_page(FMCKey key) {
    switch (key) {
    case FMC_KEY_INIT_REF: return PAGE_INIT_REF;
    case FMC_KEY_RTE:      return PAGE_RTE;
    case FMC_KEY_CLB:      return PAGE_CLB;
    case FMC_KEY_CRZ:      return PAGE_CRZ;
    case FMC_KEY_DES:      return PAGE_DES;
    case FMC_KEY_DEP_ARR:  return PAGE_DEP_ARR;
    case FMC_KEY_LEGS:     return PAGE_LEGS;
    case FMC_KEY_HOLD:     return PAGE_HOLD;
    case FMC_KEY_PROG:     return PAGE_PROG;
    case FMC_KEY_FIX:      return PAGE_FIX;
    case FMC_KEY_NAV_RAD:  return PAGE_NAV_RAD;
    default: return PAGE_INIT_REF;
    }
}

void fmc_on_lsk(FMCButton* btn) {
    int idx; bool left;
    if (btn->key >= FMC_KEY_L1 && btn->key <= FMC_KEY_L6) {
        idx = btn->key - FMC_KEY_L1; left = true;
    } else {
        idx = btn->key - FMC_KEY_R1; left = false;
    }

    // === INDEX 主页 LSK 处理 ===
    if (g_screen.current_page == PAGE_INDEX) {
        if (left && idx == 0) {
            // L1 <STATUS → 进入 IDENT 页面 (NAV DATA 状态)
            fmc_switch_page(PAGE_INIT_REF);
            g_init_subpage = 1;
            page_draw_init_ref(&g_screen);
        }
        else if (!left && idx == 1) {
            // R2 ROUTE MENU> → 进入航路页面
            fmc_switch_page(PAGE_RTE);
        }
        else if (!left && idx == 2) {
            // R3 DATABASE> → 进入 IDENT 页面 (数据库信息)
            fmc_switch_page(PAGE_INIT_REF);
            g_init_subpage = 1;
            page_draw_init_ref(&g_screen);
        }
        else if (!left && idx == 3) {
            // R4 ARR DATE> → 显示到达预测 (PROG 页面)
            fmc_switch_page(PAGE_PROG);
        }
        // 同步全局状态
        snprintf(fmc_title, 32, "%s", g_pages[g_screen.current_page].title);
        snprintf(fmc_scratchpad, 32, "%s", g_screen.scratchpad);
        for (int i=0;i<6;i++) {
            snprintf(fmc_lines_L[i],24,"%s",g_screen.line_L[i]);
            snprintf(fmc_lines_R[i],24,"%s",g_screen.line_R[i]);
        }
        btn->state &= ~FMC_STATE_PRESSED;
        return;
    }

    // === INIT/REF 页面: L1 切换 IDENT 子页面 ===
    if (g_screen.current_page == PAGE_INIT_REF && left && idx == 0) {
        g_init_subpage = g_init_subpage ? 0 : 1;
        page_draw_init_ref(&g_screen);
    }
    // IDENT/STATUS 子页面: L6 <INDEX 返回 INDEX 主页
    else if (g_screen.current_page == PAGE_INIT_REF && g_init_subpage == 1 && left && idx == 5) {
        g_init_subpage = 0;
        fmc_switch_page(PAGE_INDEX);
    }
    // IDENT/STATUS 子页面: R6 DATABASE>
    else if (g_screen.current_page == PAGE_INIT_REF && g_init_subpage == 1 && !left && idx == 5) {
        // DATABASE> 暂留当前页 (可扩展为数据库管理)
    }

    // === DEP/ARR 页面: INDEX→DEP→ARR (按PPT Day09规范重构) ===
    else if (g_screen.current_page == PAGE_DEP_ARR) {
        if (g_deparr_mode == 0) {
            // INDEX页: L1=<DEP (离场), L2=<ARR (进场目标机场)
            if (left && idx == 1) {
                // 进入离场模式
                g_deparr_mode = 'D';
                DepArrSelection& sel = g_select_dep_arr[0];
                sel.clear();
                const char* apt = g_screen.origin[0] ? g_screen.origin : "KSEA";
                // 查询该机场的跑道和SID
                query_runway_proc_by_airport(apt,
                    sel.runways, sel.rwy_count,
                    sel.procs,   sel.proc_count,
                    ELEM_SID, 20);
            }
            if (left && idx == 2) {
                // 进入进场模式 (目标机场)
                g_deparr_mode = 'A';
                DepArrSelection& sel = g_select_dep_arr[1];
                sel.clear();
                const char* apt = g_screen.dest[0] ? g_screen.dest : "KBFI";
                // 查询STAR+APPROACH
                query_runway_proc_by_airport(apt,
                    sel.runways, sel.rwy_count,
                    sel.procs,   sel.proc_count,
                    ELEM_STAR, 20);
                // 追加APPROACH (后续调用会追加到 procs 数组末尾)
                query_runway_proc_by_airport(apt,
                    sel.runways, sel.rwy_count,
                    sel.procs,   sel.proc_count,
                    ELEM_APPROACH, 20);
            }
            if (left && idx == 5) {
                // L6 <INDEX → 返回INDEX主页
                g_deparr_mode = 0;
                fmc_switch_page(PAGE_INDEX);
                btn->state &= ~FMC_STATE_PRESSED;
                return;
            }
        } else if (g_deparr_mode == 'D') {
            // === 离场选择 ===
            DepArrSelection& sel = g_select_dep_arr[0];
            const char* apt = g_screen.origin[0] ? g_screen.origin : "KSEA";

            // 点击跑道 (右侧 R2-R5)
            if (!left && idx >= 2 && idx <= 5) {
                int ri = idx - 2;
                if (ri < sel.rwy_count) {
                    if (sel.select_runway[0] && strcmp(sel.select_runway, sel.runways[ri]) == 0)
                        sel.select_runway[0] = '\0';  // 取消
                    else {
                        strncpy(sel.select_runway, sel.runways[ri], 15);
                        sel.select_runway[15] = '\0';
                    }
                }
            }

            // 点击程序 (左侧 L2-L5)
            if (left && idx >= 2 && idx <= 5) {
                int pi = idx - 2;
                if (pi < sel.proc_count) {
                    if (sel.select_proc[0] && strcmp(sel.select_proc, sel.procs[pi]) == 0)
                        sel.select_proc[0] = '\0';  // 取消
                    else {
                        strncpy(sel.select_proc, sel.procs[pi], 15);
                        sel.select_proc[15] = '\0';

                        // 查询该程序的过渡点
                        sel.trans_count = query_trans_by_proc(apt, sel.select_proc,
                            sel.trans, 20);

                        // 查询该程序关联的跑道 → 更新右侧跑道列表
                        sel.rwy_count = query_runway_by_proc(apt, sel.select_proc,
                            sel.runways, 20);

                        // 默认选中第一个过渡点
                        if (sel.trans_count > 0 && !sel.select_trans[0]) {
                            strncpy(sel.select_trans, sel.trans[0], 15);
                            sel.select_trans[15] = '\0';
                        }
                    }
                }
            }

            // 点击过渡点 (左侧 L3-L5 区域的TRANS行)
            if (left && sel.select_proc[0]) {
                if (idx == 3 && sel.trans_count > 1) {
                    // 切换过渡点
                    static int trans_sel = 0;
                    trans_sel = (trans_sel + 1) % sel.trans_count;
                    strncpy(sel.select_trans, sel.trans[trans_sel], 15);
                    sel.select_trans[15] = '\0';
                }
            }

            // 如果程序和跑道都已选, 点亮EXEC
            if (sel.select_proc[0] && sel.select_runway[0]) {
                g_screen.route_ready = true;
                g_screen.exec_light = true;
                fmc_exec_light = true;
            }
        } else {
            // === 进场选择 ===
            DepArrSelection& sel = g_select_dep_arr[1];
            const char* apt = g_screen.dest[0] ? g_screen.dest : "KBFI";

            // 点击跑道 (右侧)
            if (!left && idx >= 2 && idx <= 5) {
                int ri = idx - 2;
                if (ri < sel.rwy_count) {
                    if (sel.select_runway[0] && strcmp(sel.select_runway, sel.runways[ri]) == 0)
                        sel.select_runway[0] = '\0';
                    else {
                        strncpy(sel.select_runway, sel.runways[ri], 15);
                        sel.select_runway[15] = '\0';
                    }
                }
            }

            // 点击程序 (左侧)
            if (left && idx >= 2 && idx <= 5) {
                int pi = idx - 2;
                if (pi < sel.proc_count) {
                    if (sel.select_proc[0] && strcmp(sel.select_proc, sel.procs[pi]) == 0)
                        sel.select_proc[0] = '\0';
                    else {
                        strncpy(sel.select_proc, sel.procs[pi], 15);
                        sel.select_proc[15] = '\0';

                        sel.trans_count = query_trans_by_proc(apt, sel.select_proc,
                            sel.trans, 20);
                        sel.rwy_count = query_runway_by_proc(apt, sel.select_proc,
                            sel.runways, 20);

                        if (sel.trans_count > 0 && !sel.select_trans[0]) {
                            strncpy(sel.select_trans, sel.trans[0], 15);
                            sel.select_trans[15] = '\0';
                        }
                    }
                }
            }

            if (sel.select_proc[0] || sel.select_runway[0]) {
                g_screen.route_ready = true;
                g_screen.exec_light = true;
                fmc_exec_light = true;
            }
        }

        page_draw_dep_arr(&g_screen);
    }

    // === RTE页面: 机场/航班号输入 + 航段管理 ===
    else if (g_screen.current_page == PAGE_RTE) {
        bool route_changed = false;

        // -- 翻页 (L6/R6) --
        if (idx == 5 && left) {
            if (g_route.can_prev()) { g_route.prev_page(); page_draw_rte(&g_screen); }
            else { snprintf(g_screen.scratchpad, 32, "FIRST PAGE"); }
        }
        if (idx == 5 && !left) {
            if (g_route.can_next()) { g_route.next_page(); page_draw_rte(&g_screen); }
            else { snprintf(g_screen.scratchpad, 32, "LAST PAGE"); }
        }

        // -- 航路点输入 (L5=VIA行, 仅输入模式) --
        if (idx == 4 && left && g_screen.scratchpad[0] && g_route.current_page == 0) {
            AVLNode* found = g_waypoint_tree.search(g_screen.scratchpad);
            if (found && (found->wpt.type == 'F' || found->wpt.type == 'V')) {
                g_route.append_leg(g_screen.scratchpad, found->wpt.lat, found->wpt.lon);
                g_screen.clear_scratchpad();
                route_changed = true;
                g_route.current_page = g_route.total_pages() - 1;
            } else {
                snprintf(g_screen.scratchpad, 32, "NOT IN DB");
            }
        }

        // -- L1=ORIGIN, R1=DEST, L3=FLT_NO (仅输入模式 page 0) --
        if (g_route.current_page == 0) {
            if (idx == 0 && left && g_screen.scratchpad[0]) {
                AVLNode* found = g_waypoint_tree.search(g_screen.scratchpad);
                if (found && found->wpt.type == 'A') {
                    strncpy(g_screen.origin, g_screen.scratchpad, 7);
                    g_screen.clear_scratchpad(); route_changed = true;
                } else { snprintf(g_screen.scratchpad, 32, "NOT IN DB"); }
            }
            if (idx == 0 && !left && g_screen.scratchpad[0]) {
                AVLNode* found = g_waypoint_tree.search(g_screen.scratchpad);
                if (found && found->wpt.type == 'A') {
                    strncpy(g_screen.dest, g_screen.scratchpad, 7);
                    g_screen.clear_scratchpad(); route_changed = true;
                } else { snprintf(g_screen.scratchpad, 32, "NOT IN DB"); }
            }
            if (idx == 2 && left && g_screen.scratchpad[0]) {
                strncpy(g_screen.flt_no, g_screen.scratchpad, 15);
                g_screen.clear_scratchpad(); route_changed = true;
            }
        }

        // 每次修改后检查
        if (route_changed) {
            g_screen.route_ready = (g_screen.origin[0] && g_screen.dest[0] && g_screen.flt_no[0]);
            g_screen.exec_light = g_screen.route_ready;
            fmc_exec_light = g_screen.exec_light;
        }

        page_draw_rte(&g_screen);
        if (route_changed) fmc_route_sync_call();
    } else {
        g_screen.lsk_press(idx, left);
    }

    snprintf(fmc_scratchpad, 32, "%s", g_screen.scratchpad);
    for (int i=0;i<6;i++) {
        snprintf(fmc_lines_L[i],24,"%s",g_screen.line_L[i]);
        snprintf(fmc_lines_R[i],24,"%s",g_screen.line_R[i]);
    }
    btn->state &= ~FMC_STATE_PRESSED;
}

void fmc_on_func_key(FMCButton* btn) {
    // PREV PAGE / NEXT PAGE: 在航路相关页面翻页
    if (btn->key == FMC_KEY_PREV_PAGE &&
        (g_screen.current_page == PAGE_RTE || g_screen.current_page == PAGE_LEGS)) {
        if (g_route.can_prev()) g_route.prev_page();
        else snprintf(g_screen.scratchpad, 32, "FIRST PAGE");
        if (g_screen.current_page == PAGE_RTE) page_draw_rte(&g_screen);
        else page_draw_legs(&g_screen);
        snprintf(fmc_title, 32, "%s", g_pages[g_screen.current_page].title);
        snprintf(fmc_scratchpad, 32, "%s", g_screen.scratchpad);
        btn->state &= ~FMC_STATE_PRESSED;
        return;
    }
    if (btn->key == FMC_KEY_NEXT_PAGE &&
        (g_screen.current_page == PAGE_RTE || g_screen.current_page == PAGE_LEGS)) {
        if (g_route.can_next()) g_route.next_page();
        else snprintf(g_screen.scratchpad, 32, "LAST PAGE");
        if (g_screen.current_page == PAGE_RTE) page_draw_rte(&g_screen);
        else page_draw_legs(&g_screen);
        snprintf(fmc_title, 32, "%s", g_pages[g_screen.current_page].title);
        snprintf(fmc_scratchpad, 32, "%s", g_screen.scratchpad);
        btn->state &= ~FMC_STATE_PRESSED;
        return;
    }

    // PREV/NEXT: VNAV页面循环 CLB(1/3)↔CRZ(2/3)↔DES(3/3)
    if (btn->key == FMC_KEY_PREV_PAGE && g_screen.current_page >= PAGE_CLB &&
        g_screen.current_page <= PAGE_DES) {
        FMCPage p = g_screen.current_page;
        if (p == PAGE_CLB) p = PAGE_DES; else if (p == PAGE_CRZ) p = PAGE_CLB; else p = PAGE_CRZ;
        fmc_switch_page(p);
        btn->state &= ~FMC_STATE_PRESSED;
        return;
    }
    if (btn->key == FMC_KEY_NEXT_PAGE && g_screen.current_page >= PAGE_CLB &&
        g_screen.current_page <= PAGE_DES) {
        FMCPage p = g_screen.current_page;
        if (p == PAGE_CLB) p = PAGE_CRZ; else if (p == PAGE_CRZ) p = PAGE_DES; else p = PAGE_CLB;
        fmc_switch_page(p);
        btn->state &= ~FMC_STATE_PRESSED;
        return;
    }

    // 其他功能键: 页面切换
    FMCPage page = func_to_page(btn->key);
    fmc_switch_page(page);
    snprintf(fmc_title, 32, "%s", g_pages[page].title);
    snprintf(fmc_scratchpad, 32, "%s", g_screen.scratchpad);
    for (int i=0;i<6;i++) {
        snprintf(fmc_lines_L[i],24,"%s",g_screen.line_L[i]);
        snprintf(fmc_lines_R[i],24,"%s",g_screen.line_R[i]);
    }
    btn->state &= ~FMC_STATE_PRESSED;
}

void fmc_on_letter(FMCButton* btn) {
    g_screen.type_char(btn->label[0]);
    snprintf(fmc_scratchpad, 32, "%s", g_screen.scratchpad);
    btn->state &= ~FMC_STATE_PRESSED;
}

void fmc_on_number(FMCButton* btn) {
    const char* map[12]={"1","2","3","4","5","6","7","8","9",".","0","+/-"};
    int nidx=btn->key-FMC_KEY_1;
    if (map[nidx][0]=='+') { strcat(g_screen.scratchpad,"+/-"); }
    else { int l=strlen(g_screen.scratchpad); if(l<23){g_screen.scratchpad[l]=map[nidx][0];g_screen.scratchpad[l+1]='\0';} }
    snprintf(fmc_scratchpad, 32, "%s", g_screen.scratchpad);
    btn->state &= ~FMC_STATE_PRESSED;
}

void fmc_on_edit(FMCButton* btn) {
    switch (btn->key) {
    case FMC_KEY_SP: g_screen.type_char(' '); break;
    case FMC_KEY_DEL: g_screen.backspace(); break;
    case FMC_KEY_SLASH: g_screen.type_char('/'); break;
    case FMC_KEY_CLR: g_screen.clear_scratchpad(); break;
    default: break;
    }
    snprintf(fmc_scratchpad, 32, "%s", g_screen.scratchpad);
    btn->state &= ~FMC_STATE_PRESSED;
}

void fmc_on_exec(FMCButton* btn) {
    // RTE页: 若route_ready, EXEC执行同步
    if (g_screen.current_page == PAGE_RTE && g_screen.route_ready) {
        g_screen.route_ready = false;
        g_screen.exec_light = false;
        fmc_exec_light = false;
        btn->state = FMC_STATE_NORMAL;

        // 同步到共享内存 + X-Plane FMC
        fmc_route_sync_call();
        fmc_xplane_sync_call(g_screen.origin, g_screen.dest, g_screen.flt_no);

        snprintf(g_screen.scratchpad, 32, "ROUTE SYNCED");
        snprintf(fmc_scratchpad, 32, "%s", g_screen.scratchpad);
        return;
    }

    g_screen.exec_light = !g_screen.exec_light;
    fmc_exec_light = g_screen.exec_light;
    btn->state = g_screen.exec_light ? FMC_STATE_ACTIVE : FMC_STATE_NORMAL;

    if (g_screen.exec_light && g_screen.scratchpad[0]) {
        char* val = g_screen.scratchpad;
        FMCPage pg = g_screen.current_page;

        // === 边界校验通用函数 ===
        auto valid_alt = [](const char* s) {
            if (s[0]=='F'&&s[1]=='L') { int a=atoi(s+2); return a>=50&&a<=410; }
            int a=atoi(s); return a>=1000&&a<=41000;
        };
        auto valid_spd = [](const char* s) { int v=atoi(s); return v>=100&&v<=340; };
        auto valid_ci  = [](const char* s) { int c=atoi(s); return c>=0&&c<=500; };

        if (pg == PAGE_CLB) {
            // TGT SPEED: "290"→改knots, "/.76"→改mach
            if (val[0] == '/' && val[1] == '.') {
                float m = atof(val + 1);
                if (m >= 0.40f && m <= 0.95f) {
                    char buf[8]; snprintf(buf, 8, "290/%.2f", m);
                    strncpy(g_screen.clb_tgt_spd, buf, 7);
                }
            } else if (atoi(val) >= 100 && atoi(val) <= 399) {
                const char* slash = strchr(g_screen.clb_tgt_spd, '/');
                char buf[8];
                snprintf(buf, 8, "%d%s", atoi(val), slash ? slash : "/.74");
                strncpy(g_screen.clb_tgt_spd, buf, 7);
            }
            // SPD/ALT LIMIT
            if (strchr(val, '/')) strncpy(g_screen.clb_spd_rest, val, 15);
            // TRANS ALT
            if (valid_alt(val)) strncpy(g_screen.clb_trans_alt, val, 7);
        } else if (pg == PAGE_CRZ) {
            // TGT SPEED
            if (val[0] == '/' && val[1] == '.') {
                float m = atof(val + 1);
                if (m >= 0.40f && m <= 0.95f) {
                    char buf[8]; snprintf(buf, 8, "300/%.2f", m);
                    strncpy(g_screen.crz_tgt_spd, buf, 7);
                }
            } else if (atoi(val) >= 100 && atoi(val) <= 399) {
                const char* slash = strchr(g_screen.crz_tgt_spd, '/');
                char buf[8];
                snprintf(buf, 8, "%d%s", atoi(val), slash ? slash : "/.74");
                strncpy(g_screen.crz_tgt_spd, buf, 7);
            }
            // CRZ ALT
            if (valid_alt(val)) strncpy(g_screen.crz_alt, val, 7);
        } else if (pg == PAGE_DES) {
            // TGT SPEED
            if (val[0] == '/' && val[1] == '.') {
                float m = atof(val + 1);
                if (m >= 0.40f && m <= 0.95f) {
                    char buf[8]; snprintf(buf, 8, "747/%.2f", m);
                    strncpy(g_screen.des_tgt_spd, buf, 7);
                }
            } else if (atoi(val) >= 100 && atoi(val) <= 399) {
                const char* slash = strchr(g_screen.des_tgt_spd, '/');
                char buf[8];
                snprintf(buf, 8, "%d%s", atoi(val), slash ? slash : "/.290");
                strncpy(g_screen.des_tgt_spd, buf, 7);
            }
            // TRANS FL (FL290 = 29000ft)
            if (val[0]=='F'&&val[1]=='L') { int a=atoi(val+2); if(a>=50&&a<=410) strncpy(g_screen.des_trans_fl, val, 7); }
            else if (atoi(val) >= 1000) { snprintf(g_screen.des_trans_fl, 8, "FL%d", atoi(val)/100); }
            // SPD/ALT LIMIT: 同步到CLB
            if (strchr(val, '/')) strncpy(g_screen.clb_spd_rest, val, 15);
            // VPA (0.5~6.0°)
            float v = atof(val);
            if (v >= 0.5f && v <= 6.0f) { snprintf(g_screen.des_vpa, 8, "%.1f", v); strcat(g_screen.des_vpa, "\xb0"); }
        } else if (pg == PAGE_RTE) {
            // 原有RTE逻辑
            for (int i=0;i<6;i++) {
                if (strcmp(g_screen.line_L[i],"ORIGIN")==0) strncpy(g_screen.origin, val, 7);
                if (strcmp(g_screen.line_L[i],"DEST")==0)   strncpy(g_screen.dest, val, 7);
                if (strcmp(g_screen.line_L[i],"FLT NO")==0) strncpy(g_screen.flt_no, val, 15);
            }
        }

        // 重新绘制当前页面
        g_pages[pg].draw(&g_screen);
        snprintf(g_screen.scratchpad, 32, "EXECUTED");

        // 航路数据变更后同步到共享内存 (供ND读取)
        if (pg == PAGE_RTE || pg == PAGE_DEP_ARR || pg == PAGE_LEGS) {
            fmc_route_sync_call();
        }
    }
    snprintf(fmc_scratchpad, 32, "%s", g_screen.scratchpad);
}

// ============================================================
// 独立回调包装函数 (每个按键一个，内部调用分组逻辑)
// ============================================================

// ---- 屏幕左右选择键 L1-L6 ----
void fmc_handle_screen_l1(FMCButton* btn) { fmc_on_lsk(btn); }
void fmc_handle_screen_l2(FMCButton* btn) { fmc_on_lsk(btn); }
void fmc_handle_screen_l3(FMCButton* btn) { fmc_on_lsk(btn); }
void fmc_handle_screen_l4(FMCButton* btn) { fmc_on_lsk(btn); }
void fmc_handle_screen_l5(FMCButton* btn) { fmc_on_lsk(btn); }
void fmc_handle_screen_l6(FMCButton* btn) { fmc_on_lsk(btn); }

// ---- 屏幕左右选择键 R1-R6 ----
void fmc_handle_screen_r1(FMCButton* btn) { fmc_on_lsk(btn); }
void fmc_handle_screen_r2(FMCButton* btn) { fmc_on_lsk(btn); }
void fmc_handle_screen_r3(FMCButton* btn) { fmc_on_lsk(btn); }
void fmc_handle_screen_r4(FMCButton* btn) { fmc_on_lsk(btn); }
void fmc_handle_screen_r5(FMCButton* btn) { fmc_on_lsk(btn); }
void fmc_handle_screen_r6(FMCButton* btn) { fmc_on_lsk(btn); }

// ---- 功能按键 ----
void fmc_handle_init_ref(FMCButton* btn)  { fmc_on_func_key(btn); }
void fmc_handle_rte(FMCButton* btn)       { fmc_on_func_key(btn); }
void fmc_handle_clb(FMCButton* btn)       { fmc_on_func_key(btn); }
void fmc_handle_crz(FMCButton* btn)       { fmc_on_func_key(btn); }
void fmc_handle_des(FMCButton* btn)       { fmc_on_func_key(btn); }
void fmc_handle_dir_intc(FMCButton* btn)  { fmc_on_func_key(btn); }
void fmc_handle_legs(FMCButton* btn)      { fmc_on_func_key(btn); }
void fmc_handle_dep_arr(FMCButton* btn)   { fmc_on_func_key(btn); }
void fmc_handle_hold(FMCButton* btn)      { fmc_on_func_key(btn); }
void fmc_handle_prog(FMCButton* btn)      { fmc_on_func_key(btn); }
void fmc_handle_fix(FMCButton* btn)       { fmc_on_func_key(btn); }
void fmc_handle_nav_rad(FMCButton* btn)   { fmc_on_func_key(btn); }
void fmc_handle_prev_page(FMCButton* btn) { fmc_on_func_key(btn); }
void fmc_handle_next_page(FMCButton* btn) { fmc_on_func_key(btn); }

// ---- 字母按键 A-Z ----
void fmc_handle_a(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_b(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_c(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_d(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_e(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_f(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_g(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_h(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_i(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_j(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_k(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_l(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_m(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_n(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_o(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_p(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_q(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_r(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_s(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_t(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_u(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_v(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_w(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_x(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_y(FMCButton* btn) { fmc_on_letter(btn); }
void fmc_handle_z(FMCButton* btn) { fmc_on_letter(btn); }

// ---- 数字/符号按键 ----
void fmc_handle_1(FMCButton* btn)          { fmc_on_number(btn); }
void fmc_handle_2(FMCButton* btn)          { fmc_on_number(btn); }
void fmc_handle_3(FMCButton* btn)          { fmc_on_number(btn); }
void fmc_handle_4(FMCButton* btn)          { fmc_on_number(btn); }
void fmc_handle_5(FMCButton* btn)          { fmc_on_number(btn); }
void fmc_handle_6(FMCButton* btn)          { fmc_on_number(btn); }
void fmc_handle_7(FMCButton* btn)          { fmc_on_number(btn); }
void fmc_handle_8(FMCButton* btn)          { fmc_on_number(btn); }
void fmc_handle_9(FMCButton* btn)          { fmc_on_number(btn); }
void fmc_handle_0(FMCButton* btn)          { fmc_on_number(btn); }
void fmc_handle_dot(FMCButton* btn)        { fmc_on_number(btn); }
void fmc_handle_plus_minus(FMCButton* btn) { fmc_on_number(btn); }

// ---- 编辑按键 ----
void fmc_handle_sp(FMCButton* btn)    { fmc_on_edit(btn); }
void fmc_handle_del(FMCButton* btn)   { fmc_on_edit(btn); }
void fmc_handle_slash(FMCButton* btn) { fmc_on_edit(btn); }
void fmc_handle_clr(FMCButton* btn)   { fmc_on_edit(btn); }

// ---- 执行按键 ----
void fmc_handle_exec(FMCButton* btn) { fmc_on_exec(btn); }
