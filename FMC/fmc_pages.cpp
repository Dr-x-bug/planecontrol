/**
 * fmc_pages.cpp — FMC 页面绘制函数实现
 *
 * ========== 架构 ==========
 * FMC使用"页面注册表"模式: g_pages[] 数组将页面枚举映射到绘制函数,
 * fmc_switch_page() 是统一的页面切换入口。
 *
 * 每个页面绘制函数的职责:
 *   1. scr->clear_lines()         清空6行×2列显示缓冲区
 *   2. scr->set_line_L/R(row,text) 填充左/右行文本
 *   3. scr->set_line_L/R_val()    填充大字数值 (标签下方)
 *
 * ========== 页面列表 ==========
 *   INDEX      — 主页: <STATUS / ROUTE MENU> / DATABASE> / ARR DATE>
 *   INIT/REF   — 识别/参考: IDENT状态页(NAV DATA/UTC/DATE等)
 *   RTE        — 航路: ORIGIN/DEST/FLT_NO输入 + 航段浏览
 *   CLB/CRZ/DES— VNAV爬升/巡航/下降: TGT SPEED/ALT/TRANS参数
 *   DEP/ARR    — 进离场: 三级界面(INDEX→DEP/ARR→选择页)
 *   LEGS       — 航段列表
 *   PROG       — 飞行进程
 */

#include "fmc_pages.h"
#include "fmc_route.h"
#include "fmc_deparr.h"
#include <cstdio>
#include <cstring>

// ============================================================
// 全局页面状态
// ============================================================
FMCScreen g_screen;  // FMC屏幕完整状态 (6行×2列 + 草稿栏 + 临时参数)

// 页面注册表: 将FMCPage枚举映射到对应的绘制函数
PageDef g_pages[] = {
    {PAGE_INDEX,    "INDEX",          page_draw_index},
    {PAGE_INIT_REF, "INIT/REF INDEX", page_draw_init_ref},
    {PAGE_RTE,      "RTE",            page_draw_rte},
    {PAGE_CLB,      "CLB",            page_draw_clb},
    {PAGE_CRZ,      "CRZ",            page_draw_crz},
    {PAGE_DES,      "DES",            page_draw_des},
    {PAGE_DEP_ARR,  "DEP/ARR INDEX",  page_draw_dep_arr},
    {PAGE_LEGS,     "LEGS",           page_draw_legs},
    {PAGE_HOLD,     "HOLD",           page_draw_hold},
    {PAGE_PROG,     "PROG",           page_draw_prog},
    {PAGE_FIX,      "FIX",            page_draw_legs},       // 复用LEGS绘制
    {PAGE_NAV_RAD,  "NAV RAD",        page_draw_legs},       // 复用LEGS绘制
};

/**
 * fmc_switch_page — 统一的页面切换函数
 *
 * 调用时机: 用户点击功能键(INIT REF/RTE/CLB等) 或 LSK触发跳转时
 * 执行步骤:
 *   1. 更新 g_screen.current_page
 *   2. 设置 need_redraw 标志 (通知渲染器重绘)
 *   3. 清空草稿栏 (避免跨页面数据残留)
 *   4. 立即调用目标页面的 draw() 函数填充显示数据
 */
void fmc_switch_page(FMCPage page) {
    if (page >= PAGE_COUNT) return;
    g_screen.current_page = page;
    g_screen.need_redraw = true;
    g_screen.clear_scratchpad();
    g_pages[page].draw(&g_screen);
}

// INIT/REF 子页面状态: 0=主页(功能列表), 1=IDENT状态页(导航数据库详情)
int g_init_subpage = 0;

// g_deparr_mode 定义在 fmc_deparr.cpp 中 (避免重复定义)

// ===== INDEX 主页 (默认首页, 与真实FMC面板一致) =====
void page_draw_index(FMCScreen* scr) {
    scr->clear_lines();

    // L1: <STATUS
    scr->set_line_L(0, "<STATUS");
    // R2: ROUTE MENU>
    scr->set_line_R(1, "ROUTE MENU>");
    // R3: DATABASE>
    scr->set_line_R(2, "DATABASE>");
    // R4: ARR DATE>
    scr->set_line_R(3, "ARR DATE>");
    // L5, L6, R5, R6 留空
}

void page_draw_init_ref(FMCScreen* scr) {
    scr->clear_lines();

    if (g_init_subpage == 0) {
        // 主页: 功能入口列表
        scr->set_line_L(0, "<IDENT");       scr->set_line_R(0, "");
        scr->set_line_L(1, "<POS");         scr->set_line_R(1, "");
        scr->set_line_L(2, "<PERF");        scr->set_line_R(2, "");
        scr->set_line_L(3, "<THRUST LIM");  scr->set_line_R(3, "");
        scr->set_line_L(4, "<TAKEOFF");     scr->set_line_R(4, "");
        scr->set_line_L(5, "<APPROACH");    scr->set_line_R(5, "");
    } else {
        // IDENT/STATUS 状态页: 蓝色标签(上) + 白色数值(下)
        // 除UTC/DATE左右分列外, 其余均为左侧堆叠
        scr->set_line_L(0, "NAV DATA");           scr->set_line_L_val(0, "WORLD (XPLANE)");
        scr->set_line_L(1, "ACTIVATE DATABASE");  scr->set_line_L_val(1, "01FEB18 01MAR18");
        scr->set_line_L(2, "SEC DATABASE");       scr->set_line_L_val(2, "NOT AVAIL");
        scr->set_line_L(3, "UTC");                scr->set_line_L_val(3, "18:35");
        scr->set_line_R(3, "DATE");               scr->set_line_R_val(3, "22MAR25");
        scr->set_line_L(4, "PROGRAM");            scr->set_line_L_val(4, "X-PLANE 11.55r2");
        scr->set_line_L(5, "<INDEX");             scr->set_line_R(5, "DATABASE>");
    }
}

void page_draw_rte(FMCScreen* scr) {
    scr->clear_lines();
    char pg[16]; snprintf(pg, 16, "%d/%d", g_route.current_page + 1, g_route.total_pages());

    if (g_route.current_page == 0) {
        // === Page 1: L1=ORIGIN, R1=DEST, L3=FLT_NO, L5=VIA ===
        scr->set_line_R(0, pg);
        scr->set_line_L(0, "ORIGIN");     scr->set_line_L_val(0, scr->origin[0] ? scr->origin : "□□□□");
        scr->set_line_R(0, "DEST");       scr->set_line_R_val(0, scr->dest[0]   ? scr->dest   : "□□□□");
        scr->set_line_L(2, "CO ROUTE");   scr->set_line_L_val(2, "-----");
        scr->set_line_R(2, "FLT NO");     scr->set_line_R_val(2, scr->flt_no[0] ? scr->flt_no : "-----");
        scr->set_line_L(4, "VIA");        scr->set_line_R(4, "TO");
    } else {
        // === Page 2+: 航段浏览 ===
        int start = g_route.page_start(), end = g_route.page_end();
        scr->set_line_L(0, "ACT FPLN");
        scr->set_line_R(0, pg);
        scr->set_line_L(1, "VIA");
        scr->set_line_R(1, "TO");
        int row = 2;
        for (int i = start; i < end && row <= 5; i++, row++) {
            scr->set_line_L(row, g_route.legs[i].via);
            scr->set_line_R(row, g_route.legs[i].to);
        }
    }
}

void page_draw_clb(FMCScreen* scr) {
    scr->clear_lines();
    scr->set_line_L(0, "ACT VNAV CLIMB");                scr->set_line_R(0, "1/3");
    scr->set_line_L(1, "TGT SPEED");   scr->set_line_L_val(1, scr->clb_tgt_spd);
    scr->set_line_R(1, "TRANS ALT");   scr->set_line_R_val(1, scr->clb_trans_alt);
    scr->set_line_L(3, "SPD/ALT LIMIT"); scr->set_line_L_val(3, scr->clb_spd_rest);
    scr->set_line_L_val(4, scr->clb_spd_alt2);
}

void page_draw_crz(FMCScreen* scr) {
    scr->clear_lines();
    scr->set_line_L(0, "VNAV CRUISE");                scr->set_line_R(0, "2/3");
    scr->set_line_L(2, "TGT SPEED");  scr->set_line_L_val(2, scr->crz_tgt_spd);
    scr->set_line_R(2, "CRZ ALT");    scr->set_line_R_val(2, scr->crz_alt[0] ? scr->crz_alt : "-----");
}

void page_draw_des(FMCScreen* scr) {
    scr->clear_lines();
    scr->set_line_L(0, "VNAV DESCENT");               scr->set_line_R(0, "3/3");
    scr->set_line_L(2, "TGT SPEED");  scr->set_line_L_val(2, scr->des_tgt_spd);
    scr->set_line_R(2, "TRANS FL");   scr->set_line_R_val(2, scr->des_trans_fl);
    scr->set_line_L(4, "SPD/ALT LIMIT"); scr->set_line_L_val(4, scr->clb_spd_rest);
    scr->set_line_L(5, "VPA");        scr->set_line_R_val(5, scr->des_vpa);
}

void page_draw_dep_arr(FMCScreen* scr) {
    scr->clear_lines();

    if (g_deparr_mode == 0) {
        // === INDEX: DEP/ARR 入口页 ===
        scr->set_line_L(0, "DEP/ARR INDEX");          scr->set_line_R(0, "ACT FPLN");
        scr->set_line_L(1, "<DEP");
        scr->set_line_L_val(1, scr->origin[0] ? scr->origin : "KSEA");
        scr->set_line_R(1, "ARR>");
        scr->set_line_L(2, "");
        scr->set_line_L_val(2, scr->dest[0] ? scr->dest : "KBFI");
        scr->set_line_R(2, "ARR>");
        scr->set_line_L(5, "<INDEX");
    } else if (g_deparr_mode == 'D') {
        // === DEPART 离场 ===
        DepArrSelection& sel = g_select_dep_arr[0];  // [0]=离场
        const char* apt = scr->origin[0] ? scr->origin : "KSEA";

        char title[32];
        snprintf(title, 32, "%s DEPART", apt);
        scr->set_line_L(0, title);                   scr->set_line_R(0, "1/1");

        bool has_sel = (sel.select_proc[0] && sel.select_runway[0]);
        if (!has_sel) {
            // 未选: 显示全部程序 (左侧 L2-L5) + 跑道 (右侧 R2-R5)
            scr->set_line_L(1, "SIDS");                scr->set_line_R(1, "RUNWAYS");
            for (int i = 0; i < sel.proc_count && i < 4; i++)
                scr->set_line_L(2 + i, sel.procs[i]);
            for (int i = 0; i < sel.rwy_count && i < 4; i++)
                scr->set_line_R(2 + i, sel.runways[i]);
        } else {
            // 已选: 显示选中项 + 过渡点
            char buf[32];
            snprintf(buf, 32, "%s <SEL>", sel.select_proc);
            scr->set_line_L(2, buf);
            scr->set_line_L(3, "TRANS");
            scr->set_line_L(4, sel.select_trans[0] ? sel.select_trans : "-----");
            if (sel.trans_count > 1)
                scr->set_line_L(5, sel.trans[1]);

            snprintf(buf, 32, "%s <SEL>", sel.select_runway);
            scr->set_line_R(2, buf);
            scr->set_line_R(3, "TRANS");
        }
    } else {
        // === ARRIVAL 进场 ===
        DepArrSelection& sel = g_select_dep_arr[1];  // [1]=进场
        const char* apt = scr->dest[0] ? scr->dest : "KBFI";

        char title[32];
        snprintf(title, 32, "%s ARRIVAL", apt);
        scr->set_line_L(0, title);                   scr->set_line_R(0, "1/1");

        bool has_arr = (sel.select_proc[0] && sel.select_runway[0]);
        if (!has_arr) {
            // 未选: STARS(左侧) + APPROACHES(右侧)
            scr->set_line_L(1, "STARS");               scr->set_line_R(1, "APPROACHES");
            int sc = 0, ac = 0;
            for (int i = 0; i < sel.proc_count; i++) {
                // 查找该程序的类型
                char t = 0;
                for (int j = 0; j < g_element_count; j++) {
                    if (strcmp(g_elements[j].name, sel.procs[i]) == 0 &&
                        strcmp(g_elements[j].airport, apt) == 0) {
                        t = g_elements[j].type; break;
                    }
                }
                if (t == ELEM_STAR && sc < 4)
                    scr->set_line_L(2 + sc++, sel.procs[i]);
                else if (t == ELEM_APPROACH && ac < 4)
                    scr->set_line_R(2 + ac++, sel.procs[i]);
            }
            // 也显示跑道
            for (int i = 0; i < sel.rwy_count && i < 4; i++)
                scr->set_line_R(2 + i, sel.runways[i]);
        } else {
            char buf[32];
            snprintf(buf, 32, "%s <SEL>", sel.select_proc);
            scr->set_line_L(2, buf);
            scr->set_line_L(3, "TRANS");
            scr->set_line_L(4, sel.select_trans[0] ? sel.select_trans : "-----");
            if (sel.trans_count > 1)
                scr->set_line_L(5, sel.trans[1]);

            snprintf(buf, 32, "%s <SEL>", sel.select_runway);
            scr->set_line_R(2, buf);
            scr->set_line_R(3, "TRANS");
        }
    }
}

void page_draw_legs(FMCScreen* scr) {
    scr->clear_lines();
    // 使用航路数据填充LEGS页面
    if (g_route.count > 0) {
        for (int i = 0; i < 6 && i < g_route.count; i++) {
            scr->set_line_L(i, g_route.wpts[i].id);
            char info[24];
            snprintf(info, 24, "%03d/FL350", (47 + i * 30) % 360);
            scr->set_line_R(i, info);
        }
    } else {
        scr->set_line_L(0, scr->origin[0]?scr->origin:"KSEA");
        scr->set_line_R(0, "252/FL350");
        scr->set_line_L(1, "BLI");       scr->set_line_R(1, "081/FL350");
        scr->set_line_L(2, "FREDY");     scr->set_line_R(2, "360/FL350");
        scr->set_line_L(3, "RENTO");     scr->set_line_R(3, "302/FL350");
        scr->set_line_L(4, "TOTEM");     scr->set_line_R(4, "255/FL320");
        scr->set_line_L(5, scr->dest[0]?scr->dest:"KBFI"); scr->set_line_R(5, "205/FL280");
    }
}

void page_draw_hold(FMCScreen* scr) {
    scr->clear_lines();
    scr->set_line_L(0, "HOLD AT");       scr->set_line_R(0, "-----");
    scr->set_line_L(1, "INBOUND");       scr->set_line_R(1, "270/LEFT");
    scr->set_line_L(2, "TURN DIR");      scr->set_line_R(2, "RIGHT");
    scr->set_line_L(3, "LEG TIME");      scr->set_line_R(3, "1.0 MIN");
    scr->set_line_L(4, "LEG DIST");      scr->set_line_R(4, "-----");
    scr->set_line_L(5, "FIX ETA");       scr->set_line_R(5, "-----");
}

void page_draw_prog(FMCScreen* scr) {
    scr->clear_lines();
    scr->set_line_L(0, "LAST POS");      scr->set_line_R(0, "KSEA");
    scr->set_line_L(1, "NEXT");          scr->set_line_R(1, "BLI");
    scr->set_line_L(2, "DIST TO DEST");  scr->set_line_R(2, "85.2 NM");
    scr->set_line_L(3, "ETA");           scr->set_line_R(3, "1432Z");
    scr->set_line_L(4, "FUEL REM");      scr->set_line_R(4, "6800 KG");
    scr->set_line_L(5, "WIND");          scr->set_line_R(5, "270/15KT");
}

// ===== FMC 屏幕渲染 =====
void fmc_draw_screen(FMCRenderer& r) {
    // LED屏幕深色背景 (居中于LSK按钮之间)
    r.fill_rect(98, 75, 430, 355, {1, 2, 1, 255});

    // 第1行: 页面标题(中) — 青绿色大字
    r.draw_text_center(315, 90, g_pages[g_screen.current_page].title, Color::FMC_CYAN, false);

    // 6行文字, 与LSK按钮对齐
    // STATUS页: 蓝色小字标签(上) + 白色大字数值(下) 同行双行
    // DEP/ARR 专用: 双行渲染(青绿标签+白数据) + 虚线分隔
    bool is_deparr = (g_screen.current_page == PAGE_DEP_ARR);
    // 双行: STATUS 或 RTE输入模式(无航段时)
    // 双行: STATUS(蓝标签+白值) 或 RTE输入(全白大字)
    bool is_dual     = (g_screen.current_page == PAGE_INIT_REF && g_init_subpage == 1)
                     || (g_screen.current_page == PAGE_CLB)
                     || (g_screen.current_page == PAGE_CRZ)
                     || (g_screen.current_page == PAGE_DES);
    bool is_rte_dual = (g_screen.current_page == PAGE_RTE && g_route.current_page == 0);
    bool is_rte_legs = (g_screen.current_page == PAGE_RTE && g_route.current_page > 0);
    bool is_index  = (g_screen.current_page == PAGE_INDEX);
    SDL_Color line_c = is_index ? Color::FMC_WHITE : Color::FMC_CYAN;
    bool line_sm    = is_index ? false : true;

    // 6行y坐标 (对齐LSK按钮中心: L1≈140, L2≈188, L3≈236, L4≈284, L5≈332, L6≈380)
    int ly[6] = {140, 188, 236, 284, 332, 380};
    for (int i = 0; i < 6; i++) {
        if (is_dual) {
            // STATUS: 蓝色标签(上) + 白色数值(下)
            int ly_top = ly[i] - 8, ly_val = ly[i] + 10;
            if (g_screen.line_L[i][0])
                r.draw_text(106, ly_top, g_screen.line_L[i], Color::FMC_BLUE, true);
            if (g_screen.line_L_val[i][0])
                r.draw_text(106, ly_val, g_screen.line_L_val[i], Color::FMC_WHITE, false);
            if (g_screen.line_R[i][0])
                r.draw_text_right(526, ly_top, g_screen.line_R[i], Color::FMC_BLUE, true);
            if (g_screen.line_R_val[i][0])
                r.draw_text_right(526, ly_val, g_screen.line_R_val[i], Color::FMC_WHITE, false);
        } else if (is_rte_dual) {
            // RTE输入: 全白色大字(标签+数值)
            int ly_top = ly[i] - 8, ly_val = ly[i] + 10;
            if (g_screen.line_L[i][0])
                r.draw_text(106, ly_top, g_screen.line_L[i], Color::FMC_WHITE, false);
            if (g_screen.line_L_val[i][0])
                r.draw_text(106, ly_val, g_screen.line_L_val[i], Color::FMC_WHITE, false);
            if (g_screen.line_R[i][0])
                r.draw_text_right(526, ly_top, g_screen.line_R[i], Color::FMC_WHITE, false);
            if (g_screen.line_R_val[i][0])
                r.draw_text_right(526, ly_val, g_screen.line_R_val[i], Color::FMC_WHITE, false);
        } else if (is_rte_legs) {
            // RTE航段: 全白色, 行0-1=大字标签, 行2-5=大字数值
            bool is_hdr = (i <= 1);
            if (g_screen.line_L[i][0])
                r.draw_text(106, ly[i], g_screen.line_L[i], Color::FMC_WHITE, is_hdr);
            if (g_screen.line_R[i][0])
                r.draw_text_right(526, ly[i], g_screen.line_R[i], Color::FMC_WHITE, is_hdr);
        } else if (is_deparr) {
            // DEP/ARR: 双行(青绿标签+白数据)
            int ly_top = ly[i] - 8, ly_val = ly[i] + 10;
            if (g_screen.line_L[i][0])
                r.draw_text(106, ly_top, g_screen.line_L[i], Color::FMC_CYAN, true);
            if (g_screen.line_L_val[i][0])
                r.draw_text(106, ly_val, g_screen.line_L_val[i], Color::FMC_WHITE, false);
            if (g_screen.line_R[i][0])
                r.draw_text_right(526, ly_top, g_screen.line_R[i], Color::FMC_CYAN, true);
            if (g_screen.line_R_val[i][0])
                r.draw_text_right(526, ly_val, g_screen.line_R_val[i], Color::FMC_WHITE, false);
        } else {
            // 普通单行
            if (g_screen.line_L[i][0])
                r.draw_text(106, ly[i], g_screen.line_L[i], line_c, line_sm);
            if (g_screen.line_R[i][0])
                r.draw_text_right(526, ly[i], g_screen.line_R[i], line_c, line_sm);
        }
    }

    // ===== DEP/ARR: 虚线分隔 + 方括号 =====
    if (is_deparr) {
        int dash_y = 385;
        for (int dx = 110; dx < 520; dx += 12)
            lineRGBA(r.sdl_rend, r.to_sx(dx), r.to_sy(dash_y), r.to_sx(dx+6), r.to_sy(dash_y), 0, 220, 220, 180);
        r.draw_text(106, 398, "[", Color::FMC_CYAN, false);
        r.draw_text_right(526, 398, "]", Color::FMC_CYAN, false);
    }

    // ===== RTE页底部LSK =====
    if (g_screen.current_page == PAGE_RTE) {
        if (g_route.current_page > 0) {
            char pg[24];
            snprintf(pg, 24, "%d/%d", g_route.current_page + 1, g_route.total_pages());
            if (g_route.can_prev())
                r.draw_text(106, 392, "<PREV PAGE", Color::FMC_WHITE, false);
            r.draw_text_center(315, 392, pg, Color::FMC_WHITE, false);
            if (g_route.can_next())
                r.draw_text_right(526, 392, "NEXT PAGE>", Color::FMC_WHITE, false);
        } else {
            r.draw_text(106, 392, "<ROUTE MENU", Color::FMC_WHITE, false);
            r.draw_text_right(526, 392, "ACTIVATE>", Color::FMC_WHITE, false);
        }
    }

    // ===== 草稿栏 =====
    int sy = 400;
    r.draw_text(106, sy, "<", Color::FMC_CYAN, false);
    r.draw_text_right(526, sy, ">", Color::FMC_CYAN, false);
    if (g_screen.scratchpad[0])
        r.draw_text(122, sy, g_screen.scratchpad, Color::FMC_CYAN, false);

    // EXEC 灯
    if (g_screen.exec_light || g_screen.route_ready) {
        r.draw_text(480, sy, "EXEC", Color::FMC_AMBER, false);
    }
}
