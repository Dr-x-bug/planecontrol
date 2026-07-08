#include "fmc_pages.h"
#include "fmc_route.h"
#include "fmc_deparr.h"
#include <cstdio>
#include <cstring>

FMCScreen g_screen;

PageDef g_pages[] = {
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

void page_draw_init_ref(FMCScreen* scr) {
    scr->clear_lines();
    scr->set_line_L(0, "<IDENT");       scr->set_line_R(0, "");
    scr->set_line_L(1, "<POS");         scr->set_line_R(1, "");
    scr->set_line_L(2, "<PERF");        scr->set_line_R(2, "");
    scr->set_line_L(3, "<THRUST LIM");  scr->set_line_R(3, "");
    scr->set_line_L(4, "<TAKEOFF");     scr->set_line_R(4, "");
    scr->set_line_L(5, "<APPROACH");    scr->set_line_R(5, "");
}

void page_draw_rte(FMCScreen* scr) {
    scr->clear_lines();
    int start = g_route.page_start(), end = g_route.page_end(), total = g_route.total_pages();

    // 第1行: ORIGIN | DEST
    scr->set_line_L(0, "ORIGIN");
    scr->set_line_R(0, scr->dest[0] ? scr->dest : "KBFI");

    // 第2-5行: 航路点 (每页最多显示5个, 但第1行是ORIGIN, 所以从第2行开始)
    int display_row = 1;
    for (int i = 0; i < PAGE_SIZE && display_row < 5; i++) {
        const RouteWpt* w = g_route.get_page_wpt(i);
        if (w) {
            scr->set_line_L(display_row, w->id);
            char info[24];
            snprintf(info, 24, "%.4f/%.4f", w->lat, w->lon);
            scr->set_line_R(display_row, info);
            display_row++;
        }
    }

    // 如果没有航路点
    if (g_route.count == 0) {
        scr->set_line_L(2, "-----");
        scr->set_line_R(2, "-----");
    }

    // 底部: 翻页 & ACTIVATE
    if (total > 1) {
        scr->set_line_L(5, "<PREV PAGE");
        char pg[24]; snprintf(pg, 24, "%d/%d>", g_route.current_page + 1, total);
        scr->set_line_R(5, pg);
    }
    scr->set_line_L(4, "<ACTIVATE>");
    scr->set_line_R(4, "EXEC>");
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

// ===== FMC 屏幕渲染 (匹配 Boeing CDU 风格) =====
void fmc_draw_screen(FMCRenderer& r) {
    // 深色屏幕背景
    r.fill_rect(34, 50, 570, 380, {2, 3, 2, 255});

    // 页面标题 (顶部)
    r.draw_text_center(320, 62, g_pages[g_screen.current_page].title, Color::FMC_GREEN, false);

    // 标题下分隔线
    r.draw_line_h(40, 78, 598, {0, 180, 0, 60});

    // 6行文字, 与LSK按钮中心对齐
    // LSK按钮Y: 118, 166, 214, 262, 310, 358  中心: 140, 188, 236, 284, 332, 380
    int ly[6] = {146, 194, 242, 290, 338, 386};
    for (int i = 0; i < 6; i++) {
        // 左侧文字 (小号, 绿色标签)
        if (g_screen.line_L[i][0])
            r.draw_text(44, ly[i], g_screen.line_L[i], Color::FMC_GREEN, true);
        // 右侧文字 (正常大小, 白色数据)
        if (g_screen.line_R[i][0])
            r.draw_text(360, ly[i], g_screen.line_R[i], Color::FMC_WHITE, false);
    }

    // 草稿栏分隔线
    r.draw_line_h(40, 406, 598, {0, 180, 0, 60});

    // 草稿栏 (scratchpad)
    r.fill_rect(34, 408, 570, 32, {1, 1, 1, 255});
    r.draw_text(48, 430, g_screen.scratchpad, Color::FMC_CYAN, false);

    // EXEC 指示灯
    if (g_screen.exec_light) {
        r.fill_rect(520, 408, 84, 32, {0, 50, 0, 255});
        r.draw_text(536, 430, "EXEC", Color::FMC_AMBER, false);
    }

    // 消息区 (MSG / DISCONTINUITY 等)
    r.draw_text(44, 78, g_screen.scratchpad[0] ? "MSG" : "", Color::FMC_AMBER, true);
}
