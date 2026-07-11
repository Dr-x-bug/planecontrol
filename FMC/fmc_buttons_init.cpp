#include "fmc_ui.h"
#include <cstring>

void fmc_buttons_init() {
    memset(fmc_buttons, 0, sizeof(fmc_buttons));

    // ===== 左侧 L1-L6 (x=7, w=30, 间距48) =====
    {
        const char* ll[]={"L1","L2","L3","L4","L5","L6"};
        FMCButtonCallback l_cb[]={fmc_handle_screen_l1,fmc_handle_screen_l2,fmc_handle_screen_l3,
                                   fmc_handle_screen_l4,fmc_handle_screen_l5,fmc_handle_screen_l6};
        for (int i=0;i<6;i++) {
            FMCButton* b=&fmc_buttons[FMC_KEY_L1+i];
            b->key=(FMCKey)(FMC_KEY_L1+i); b->shape=FMC_SHAPE_RECT;
            b->rect={7,118+i*48,30,44};
            b->label=ll[i]; b->on_click=l_cb[i];
            b->color_normal={50,50,50,255}; b->color_hover={80,80,120,255};
        }
    }

    // ===== 右侧 R1-R6 (x=587, w=30) =====
    {
        const char* rl[]={"R1","R2","R3","R4","R5","R6"};
        FMCButtonCallback r_cb[]={fmc_handle_screen_r1,fmc_handle_screen_r2,fmc_handle_screen_r3,
                                   fmc_handle_screen_r4,fmc_handle_screen_r5,fmc_handle_screen_r6};
        for (int i=0;i<6;i++) {
            FMCButton* b=&fmc_buttons[FMC_KEY_R1+i];
            b->key=(FMCKey)(FMC_KEY_R1+i); b->shape=FMC_SHAPE_RECT;
            b->rect={587,118+i*48,30,44};
            b->label=rl[i]; b->on_click=r_cb[i];
            b->color_normal={50,50,50,255}; b->color_hover={80,80,120,255};
        }
    }

    // ===== 功能键 (像素检测精确坐标) =====
    struct { FMCKey k; int x,y; const char* l; FMCButtonCallback cb; } fkeys[]={
        {FMC_KEY_INIT_REF, 69, 477, "INIT REF", fmc_handle_init_ref},
        {FMC_KEY_RTE,      153,477, "RTE",      fmc_handle_rte},
        {FMC_KEY_CLB,      235,477, "CLB",      fmc_handle_clb},
        {FMC_KEY_CRZ,      318,477, "CRZ",      fmc_handle_crz},
        {FMC_KEY_DES,      401,477, "DES",      fmc_handle_des},
        {FMC_KEY_DIR_INTC, 69, 536, "DIR INTC", fmc_handle_dir_intc},
        {FMC_KEY_LEGS,     153,536, "LEGS",     fmc_handle_legs},
        {FMC_KEY_DEP_ARR,  235,536, "DEP ARR",  fmc_handle_dep_arr},
        {FMC_KEY_HOLD,     318,536, "HOLD",     fmc_handle_hold},
        {FMC_KEY_PROG,     401,536, "PROG",     fmc_handle_prog},
        {FMC_KEY_FIX,      69, 595, "FIX",      fmc_handle_fix},
        {FMC_KEY_NAV_RAD,  153,595, "NAV RAD",  fmc_handle_nav_rad},
        {FMC_KEY_PREV_PAGE,69, 655, "PREV PAGE",fmc_handle_prev_page},
        {FMC_KEY_NEXT_PAGE,153,655, "NEXT PAGE",fmc_handle_next_page},
    };
    for (auto& fk : fkeys) {
        FMCButton* b=&fmc_buttons[fk.k];
        b->key=fk.k; b->shape=FMC_SHAPE_RECT;
        b->rect={fk.x, fk.y, FUNCTION_WIDTH, FUNCTION_HEIGHT};
        b->label=fk.l; b->on_click=fk.cb;
        b->color_normal={45,45,45,255}; b->color_hover={70,70,100,255};
    }

    // ===== EXEC =====
    { FMCButton* b=&fmc_buttons[FMC_KEY_EXEC];
      b->key=FMC_KEY_EXEC; b->shape=FMC_SHAPE_RECT;
      b->rect={500,550,70,34}; b->label="EXEC"; b->on_click=fmc_handle_exec;
      b->color_normal={50,50,50,255}; b->color_hover={100,100,60,255}; }

    // ===== 字母键 A-Z (5列×6行, 从x=263,y=613, 间距68×62) =====
    {
        FMCButtonCallback letter_cb[]={
            fmc_handle_a,fmc_handle_b,fmc_handle_c,fmc_handle_d,fmc_handle_e,
            fmc_handle_f,fmc_handle_g,fmc_handle_h,fmc_handle_i,fmc_handle_j,
            fmc_handle_k,fmc_handle_l,fmc_handle_m,fmc_handle_n,fmc_handle_o,
            fmc_handle_p,fmc_handle_q,fmc_handle_r,fmc_handle_s,fmc_handle_t,
            fmc_handle_u,fmc_handle_v,fmc_handle_w,fmc_handle_x,fmc_handle_y,fmc_handle_z};
        const char* letter_lbl[]={"A","B","C","D","E","F","G","H","I","J",
                                   "K","L","M","N","O","P","Q","R","S","T",
                                   "U","V","W","X","Y","Z"};
        for (int i=0;i<26;i++) {
            FMCButton* b=&fmc_buttons[FMC_KEY_A+i];
            b->key=(FMCKey)(FMC_KEY_A+i); b->shape=FMC_SHAPE_RECT;
            b->rect={263+(i%5)*68, 613+(i/5)*62, LETTER_LENGTH, LETTER_LENGTH};
            b->label=letter_lbl[i]; b->on_click=letter_cb[i];
            b->color_normal={40,40,40,255}; b->color_hover={60,60,110,255};
        }
    }

    // ===== 数字键 (圆形, 3列×4行, 从x=89,y=762, 间距62) =====
    const char* nl[]={"1","2","3","4","5","6","7","8","9",".","0","+/-"};
    FMCButtonCallback num_cb[]={
        fmc_handle_1,fmc_handle_2,fmc_handle_3,fmc_handle_4,fmc_handle_5,fmc_handle_6,
        fmc_handle_7,fmc_handle_8,fmc_handle_9,fmc_handle_dot,fmc_handle_0,fmc_handle_plus_minus};
    for (int i=0;i<12;i++) {
        FMCButton* b=&fmc_buttons[FMC_KEY_1+i];
        b->key=(FMCKey)(FMC_KEY_1+i); b->shape=FMC_SHAPE_CIRCLE;
        int cx=89+(i%3)*62, cy=762+(i/3)*62;
        b->rect={cx-NUMB_R, cy-NUMB_R, NUMB_R*2, NUMB_R*2};
        b->radius=NUMB_R; b->label=nl[i]; b->on_click=num_cb[i];
        b->color_normal={40,40,40,255}; b->color_hover={60,60,110,255};
    }

    // ===== 编辑键 (x=333,401,468,534, y=924) =====
    struct { FMCKey k; int x; const char* l; FMCButtonCallback cb; } ekeys[]={
        {FMC_KEY_SP,    333, "SP",  fmc_handle_sp},
        {FMC_KEY_DEL,   401, "DEL", fmc_handle_del},
        {FMC_KEY_SLASH, 468, "/",   fmc_handle_slash},
        {FMC_KEY_CLR,   534, "CLR", fmc_handle_clr},
    };
    for (auto& ek : ekeys) {
        FMCButton* b=&fmc_buttons[ek.k];
        b->key=ek.k; b->shape=FMC_SHAPE_RECT;
        b->rect={ek.x, 924, LETTER_LENGTH, LETTER_LENGTH};
        b->label=ek.l; b->on_click=ek.cb;
        b->color_normal={40,40,40,255}; b->color_hover={60,60,110,255};
    }
}
