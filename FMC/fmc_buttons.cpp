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

    // === DEP/ARR 页面特殊处理 ===
    if (g_screen.current_page == PAGE_DEP_ARR && left && idx == 0) {
        // L1: 重置/进入DEP选择
        g_deparr.dep_step = 0;
        g_deparr.dep_rwy[0] = '\0';
        g_deparr.dep_sid[0] = '\0';
        g_deparr.dep_trans[0] = '\0';
        page_draw_dep_arr(&g_screen);
    }
    else if (g_screen.current_page == PAGE_DEP_ARR && g_deparr.dep_step == 0) {
        // 跑道选择
        const char* rwys_L[] = {"16L","16C","16R"};
        const char* rwys_R[] = {"34L","34C","34R"};
        if (left && idx >= 1 && idx <= 3) { strncpy(g_deparr.dep_rwy, rwys_L[idx-1], 7); g_deparr.dep_step = 1; }
        if (!left && idx >= 1 && idx <= 3) { strncpy(g_deparr.dep_rwy, rwys_R[idx-1], 7); g_deparr.dep_step = 1; }
        page_draw_dep_arr(&g_screen);
    }
    else if (g_screen.current_page == PAGE_DEP_ARR && g_deparr.dep_step == 1) {
        // SID选择
        if (left && idx >= 2 && idx <= 4) {
            int cnt=0;
            for (int i=0;i<ksea_sid_count;i++) {
                bool m=(ksea_sids[i].runway[0]==0||strcmp(ksea_sids[i].runway,g_deparr.dep_rwy)==0);
                if (m) { if (cnt==idx-2) { strncpy(g_deparr.dep_sid,ksea_sids[i].name,15); g_deparr.dep_step=2; break; } cnt++; }
            }
        }
        page_draw_dep_arr(&g_screen);
    }
    else if (g_screen.current_page == PAGE_DEP_ARR && g_deparr.dep_step == 2) {
        // 过渡点选择
        if (left && idx >= 3 && idx <= 4) {
            for (int i=0;i<ksea_sid_count;i++) {
                bool m=(ksea_sids[i].runway[0]==0||strcmp(ksea_sids[i].runway,g_deparr.dep_rwy)==0);
                if (m&&strcmp(ksea_sids[i].name,g_deparr.dep_sid)==0) {
                    int ti=idx-3;
                    if (ti<ksea_sids[i].trans_count) strncpy(g_deparr.dep_trans,ksea_sids[i].transitions[ti],15);
                    break;
                }
            }
        }
        page_draw_dep_arr(&g_screen);
    }

    // === RTE页面 ===
    else if (g_screen.current_page == PAGE_RTE) {
        if (idx == 5 && left)  { g_route.prev_page(); page_draw_rte(&g_screen); }
        if (idx == 5 && !left) { g_route.next_page(); page_draw_rte(&g_screen); }
        if (idx == 4 && left)  { g_screen.clear_scratchpad(); }
        if (idx < 4) g_screen.lsk_press(idx, left);
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
            int row = -1;
            for (int i=0;i<6;i++) if (g_screen.line_L[i][0] && g_screen.scratchpad[0]) row=i;
            if (row==0 && valid_spd(val)) strncpy(g_screen.clb_spd, val, 7);
            if (row==1) strncpy(g_screen.clb_spd_rest, val, 15);
            if (row==2 && valid_alt(val)) strncpy(g_screen.clb_alt_rest, val, 7);
            if (row==3) strncpy(g_screen.clb_rate, val, 7);
            if (row==4 && valid_alt(val)) strncpy(g_screen.clb_trans_alt, val, 7);
        } else if (pg == PAGE_CRZ) {
            int row = -1;
            for (int i=0;i<6;i++) if (g_screen.line_L[i][0] && g_screen.scratchpad[0]) row=i;
            if (row==0 && valid_alt(val)) strncpy(g_screen.crz_alt, val, 7);
            if (row==1 && valid_ci(val))  strncpy(g_screen.cost_idx, val, 7);
            if (row==2) strncpy(g_screen.crz_turb_n1, val, 7);
            if (row==3 && valid_alt(val)) strncpy(g_screen.crz_opt_alt, val, 7);
            if (row==4 && valid_alt(val)) strncpy(g_screen.crz_max_alt, val, 7);
            if (row==5) strncpy(g_screen.crz_step, val, 7);
        } else if (pg == PAGE_DES) {
            int row = -1;
            for (int i=0;i<6;i++) if (g_screen.line_L[i][0] && g_screen.scratchpad[0]) row=i;
            if (row==0 && valid_spd(val)) strncpy(g_screen.des_spd, val, 7);
            if (row==1) strncpy(g_screen.des_spd_rest, val, 15);
            if (row==2 && valid_alt(val)) strncpy(g_screen.des_alt_rest, val, 7);
            if (row==3) strncpy(g_screen.des_rate, val, 7);
            if (row==4 && valid_alt(val)) strncpy(g_screen.des_trans_lvl, val, 7);
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
