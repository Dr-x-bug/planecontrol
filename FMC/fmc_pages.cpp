#include "fmc_pages.h"
#include "fmc_route.h"
#include "fmc_deparr.h"
#include <cstdio>
#include <cstring>

FMCScreen g_screen;

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
    {PAGE_FIX,      "FIX",            page_draw_legs},
    {PAGE_NAV_RAD,  "NAV RAD",        page_draw_legs},
};

void fmc_switch_page(FMCPage page) {
    if (page >= PAGE_COUNT) return;
    g_screen.current_page = page;
    g_screen.need_redraw = true;
    g_screen.clear_scratchpad();
    g_pages[page].draw(&g_screen);
}

// INIT/REF 子页面状态: 0=主页, 1=IDENT状态页
int g_init_subpage = 0;

// DEP/ARR 模式: 'D'=离场, 'A'=进场
char g_deparr_mode = 'D';

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

    // Row0 L1/R1: ORIGIN | DEST (上=蓝标签, 下=白数值)
    scr->set_line_L(0, "ORIGIN");     scr->set_line_L_val(0, scr->origin[0] ? scr->origin : "□□□□");
    scr->set_line_R(0, "DEST");       scr->set_line_R_val(0, scr->dest[0]   ? scr->dest   : "□□□□");

    // Row2 L2/R3: CO ROUTE | FLT NO
    scr->set_line_L(2, "CO ROUTE");   scr->set_line_L_val(2, "-----");
    scr->set_line_R(2, "FLT NO");     scr->set_line_R_val(2, scr->flt_no[0] ? scr->flt_no : "-----");

    // Row4 L5/R5: VIA | TO
    scr->set_line_L(4, "VIA");        scr->set_line_L_val(4, "-----");
    scr->set_line_R(4, "TO");         scr->set_line_R_val(4, "-----");
}

void page_draw_clb(FMCScreen* scr) {
    scr->clear_lines();
    scr->set_line_L(0, "CLB");          scr->set_line_R(0, "ECON");
    scr->set_line_L(1, "SPD RESTR");    scr->set_line_R(1, scr->clb_spd_rest);
    scr->set_line_L(2, "ALT RESTR");    scr->set_line_R(2, scr->clb_alt_rest[0]?scr->clb_alt_rest:"-----");
    scr->set_line_L(3, "CLB RATE");     scr->set_line_R(3, scr->clb_rate[0]?scr->clb_rate:"ECON");
    scr->set_line_L(4, "TRANS ALT");    scr->set_line_R(4, scr->clb_trans_alt);
    scr->set_line_L(5, "ENG OUT ACC");  scr->set_line_R(5, "1000FT");
}

void page_draw_crz(FMCScreen* scr) {
    scr->clear_lines();
    scr->set_line_L(0, "CRZ ALT");      scr->set_line_R(0, scr->crz_alt);
    scr->set_line_L(1, "COST INDEX");   scr->set_line_R(1, scr->cost_idx);
    scr->set_line_L(2, "TURB N1");      scr->set_line_R(2, scr->crz_turb_n1);
    scr->set_line_L(3, "OPT ALT");      scr->set_line_R(3, scr->crz_opt_alt);
    scr->set_line_L(4, "MAX ALT");      scr->set_line_R(4, scr->crz_max_alt);
    scr->set_line_L(5, "STEP TO");      scr->set_line_R(5, scr->crz_step);
}

void page_draw_des(FMCScreen* scr) {
    scr->clear_lines();
    scr->set_line_L(0, "DES SPD");      scr->set_line_R(0, scr->des_spd);
    scr->set_line_L(1, "SPD RESTR");    scr->set_line_R(1, scr->des_spd_rest);
    scr->set_line_L(2, "ALT RESTR");    scr->set_line_R(2, scr->des_alt_rest[0]?scr->des_alt_rest:"-----");
    scr->set_line_L(3, "DES RATE");     scr->set_line_R(3, scr->des_rate[0]?scr->des_rate:"ECON");
    scr->set_line_L(4, "TRANS LVL");    scr->set_line_R(4, scr->des_trans_lvl);
    scr->set_line_L(5, "DECEL");        scr->set_line_R(5, "10NM");
}

void page_draw_dep_arr(FMCScreen* scr) {
    scr->clear_lines();

    if (g_deparr_mode == 'D') {
        // === 离场 (DEP KSEA) ===
        scr->set_line_L(0, "<DEP");          scr->set_line_R(0, "ARR>");
        if (g_deparr.dep_step == 0) {
            scr->set_line_L(1, "ORIGIN");    scr->set_line_R(1, "KSEA");
            scr->set_line_L(2, "RWY");
            scr->set_line_L(3, " 16L");      scr->set_line_R(3, "34L");
            scr->set_line_L(4, " 16C");      scr->set_line_R(4, "34C");
            scr->set_line_L(5, " 16R");      scr->set_line_R(5, "34R");
        } else if (g_deparr.dep_step == 1) {
            scr->set_line_L(0, "<DEP");      scr->set_line_R(0, "KSEA>");
            scr->set_line_L(1, "RWY");       scr->set_line_R(1, g_deparr.dep_rwy);
            scr->set_line_L(2, "SID");       scr->set_line_R(2, g_deparr.dep_sid[0]?g_deparr.dep_sid:"----");
            int cnt=0;
            for (int i=0;i<ksea_sid_count&&cnt<3;i++) {
                bool m=(ksea_sids[i].runway[0]==0||strcmp(ksea_sids[i].runway,g_deparr.dep_rwy)==0);
                if (m) {
                    if (cnt==0) scr->set_line_L(3,ksea_sids[i].name);
                    else if(cnt==1) scr->set_line_L(4,ksea_sids[i].name);
                    else scr->set_line_L(5,ksea_sids[i].name);
                    cnt++;
                }
            }
        } else {
            scr->set_line_L(1, "RWY");       scr->set_line_R(1, g_deparr.dep_rwy);
            scr->set_line_L(2, "SID");       scr->set_line_R(2, g_deparr.dep_sid);
            scr->set_line_L(3, "TRANS");     scr->set_line_R(3, g_deparr.dep_trans[0]?g_deparr.dep_trans:"----");
            for (int i=0;i<ksea_sid_count;i++) {
                bool ok=(ksea_sids[i].runway[0]==0||strcmp(ksea_sids[i].runway,g_deparr.dep_rwy)==0);
                if (ok&&strcmp(ksea_sids[i].name,g_deparr.dep_sid)==0) {
                    for (int j=0;j<ksea_sids[i].trans_count&&j<2;j++) {
                        if (j==0) scr->set_line_L(4,ksea_sids[i].transitions[j]);
                        else scr->set_line_L(5,ksea_sids[i].transitions[j]);
                    }
                    break;
                }
            }
        }
    } else {
        // === 进场 (ARR KBFI) ===
        scr->set_line_L(0, "DEP>");          scr->set_line_R(0, "<ARR");
        if (g_deparr.arr_step == 0) {
            scr->set_line_L(1, "DEST");      scr->set_line_R(1, "KBFI");
            scr->set_line_L(2, "RWY");
            scr->set_line_L(3, " 13L");      scr->set_line_R(3, "31L");
            scr->set_line_L(4, " 13R");      scr->set_line_R(4, "31R");
            scr->set_line_L(5, "");          scr->set_line_R(5, "");
        } else if (g_deparr.arr_step == 1) {
            scr->set_line_L(0, "KBFI>");     scr->set_line_R(0, "<ARR");
            scr->set_line_L(1, "RWY");       scr->set_line_R(1, g_deparr.arr_rwy);
            scr->set_line_L(2, "STAR");      scr->set_line_R(2, g_deparr.arr_star[0]?g_deparr.arr_star:"----");
            int cnt=0;
            for (int i=0;i<kbfi_star_count&&cnt<3;i++) {
                if (kbfi_stars[i].type=='T') {
                    bool m=(kbfi_stars[i].runway[0]==0||strcmp(kbfi_stars[i].runway,g_deparr.arr_rwy)==0);
                    if (m) {
                        if (cnt==0) scr->set_line_L(3,kbfi_stars[i].name);
                        else if(cnt==1) scr->set_line_L(4,kbfi_stars[i].name);
                        else scr->set_line_L(5,kbfi_stars[i].name);
                        cnt++;
                    }
                }
            }
        } else {
            scr->set_line_L(1, "RWY");       scr->set_line_R(1, g_deparr.arr_rwy);
            scr->set_line_L(2, "STAR");      scr->set_line_R(2, g_deparr.arr_star);
            scr->set_line_L(3, "APPR");      scr->set_line_R(3, g_deparr.arr_appr[0]?g_deparr.arr_appr:"----");
            int cnt=0;
            for (int i=0;i<kbfi_star_count;i++) {
                if (kbfi_stars[i].type=='A') {
                    bool m=(kbfi_stars[i].runway[0]==0||strcmp(kbfi_stars[i].runway,g_deparr.arr_rwy)==0);
                    if (m && cnt<2) {
                        if (cnt==0) scr->set_line_L(4,kbfi_stars[i].name);
                        else scr->set_line_L(5,kbfi_stars[i].name);
                        cnt++;
                    }
                }
            }
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
    // RTE页:   偶数行蓝色标签, 奇数行白色数值 (交替)
    // INDEX主页: 白色大字; 其他页: 青绿色小字
    bool is_dual  = (g_screen.current_page == PAGE_INIT_REF && g_init_subpage == 1)
                 || (g_screen.current_page == PAGE_RTE);
    bool is_index  = (g_screen.current_page == PAGE_INDEX);
    SDL_Color line_c = is_index ? Color::FMC_WHITE : Color::FMC_CYAN;
    bool line_sm    = is_index ? false : true;

    // 6行y坐标 (对齐LSK按钮中心: L1≈140, L2≈188, L3≈236, L4≈284, L5≈332, L6≈380)
    int ly[6] = {140, 188, 236, 284, 332, 380};
    for (int i = 0; i < 6; i++) {
        if (is_dual) {
            // 双行: 蓝色标签(上, y-8) + 白色数值(下, y+10)
            int ly_top = ly[i] - 8, ly_val = ly[i] + 10;
            if (g_screen.line_L[i][0])
                r.draw_text(106, ly_top, g_screen.line_L[i], Color::FMC_BLUE, true);
            if (g_screen.line_L_val[i][0])
                r.draw_text(106, ly_val, g_screen.line_L_val[i], Color::FMC_WHITE, false);
            if (g_screen.line_R[i][0])
                r.draw_text_right(526, ly_top, g_screen.line_R[i], Color::FMC_BLUE, true);
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

    // ===== RTE页底部LSK =====
    if (g_screen.current_page == PAGE_RTE) {
        r.draw_text(106, 392, "<ROUTE MENU", Color::FMC_BLUE, true);
        r.draw_text_right(526, 392, "ACTIVATE>", Color::FMC_BLUE, true);
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
