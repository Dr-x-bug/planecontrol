#include "fmc_ui.h"
#include <cstring>

void fmc_buttons_init() {
    memset(fmc_buttons, 0, sizeof(fmc_buttons));

    // ===== 左侧 L1-L6 (x=7, w=30, 间距48) =====
    for (int i=0;i<6;i++) {
        FMCButton* b=&fmc_buttons[FMC_KEY_L1+i];
        b->key=(FMCKey)(FMC_KEY_L1+i); b->shape=FMC_SHAPE_RECT;
        b->rect={7,118+i*48,30,44};
        const char* ll[]={"L1","L2","L3","L4","L5","L6"};
        b->label=ll[i]; b->on_click=fmc_on_lsk;
        b->color_normal={50,50,50,255}; b->color_hover={80,80,120,255};
    }

    // ===== 右侧 R1-R6 (x=587, w=30) =====
    for (int i=0;i<6;i++) {
        FMCButton* b=&fmc_buttons[FMC_KEY_R1+i];
        b->key=(FMCKey)(FMC_KEY_R1+i); b->shape=FMC_SHAPE_RECT;
        b->rect={587,118+i*48,30,44};
        const char* rl[]={"R1","R2","R3","R4","R5","R6"};
        b->label=rl[i]; b->on_click=fmc_on_lsk;
        b->color_normal={50,50,50,255}; b->color_hover={80,80,120,255};
    }

    // ===== 功能键 (像素检测精确坐标) =====
    struct { FMCKey k; int x,y; const char* l; } fkeys[]={
        {FMC_KEY_INIT_REF, 69, 477, "INIT REF"},
        {FMC_KEY_RTE,      153,477, "RTE"},
        {FMC_KEY_CLB,      235,477, "CLB"},
        {FMC_KEY_CRZ,      318,477, "CRZ"},
        {FMC_KEY_DES,      401,477, "DES"},
        {FMC_KEY_DIR_INTC, 69, 536, "DIR INTC"},
        {FMC_KEY_LEGS,     153,536, "LEGS"},
        {FMC_KEY_DEP_ARR,  235,536, "DEP ARR"},
        {FMC_KEY_HOLD,     318,536, "HOLD"},
        {FMC_KEY_PROG,     401,536, "PROG"},
        {FMC_KEY_FIX,      69, 595, "FIX"},
        {FMC_KEY_NAV_RAD,  153,595, "NAV RAD"},
        {FMC_KEY_PREV_PAGE,69, 655, "PREV PAGE"},
        {FMC_KEY_NEXT_PAGE,153,655, "NEXT PAGE"},
    };
    for (auto& fk : fkeys) {
        FMCButton* b=&fmc_buttons[fk.k];
        b->key=fk.k; b->shape=FMC_SHAPE_RECT;
        b->rect={fk.x, fk.y, FUNCTION_WIDTH, FUNCTION_HEIGHT};
        b->label=fk.l; b->on_click=fmc_on_func_key;
        b->color_normal={45,45,45,255}; b->color_hover={70,70,100,255};
    }

    // ===== EXEC =====
    { FMCButton* b=&fmc_buttons[FMC_KEY_EXEC];
      b->key=FMC_KEY_EXEC; b->shape=FMC_SHAPE_RECT;
      b->rect={500,550,70,34}; b->label="EXEC"; b->on_click=fmc_on_exec;
      b->color_normal={50,50,50,255}; b->color_hover={100,100,60,255}; }

    // ===== 字母键 A-Z (5列×6行, 从x=263,y=613, 间距68×62) =====
    for (int i=0;i<26;i++) {
        FMCButton* b=&fmc_buttons[FMC_KEY_A+i];
        b->key=(FMCKey)(FMC_KEY_A+i); b->shape=FMC_SHAPE_RECT;
        b->rect={263+(i%5)*68, 613+(i/5)*62, LETTER_LENGTH, LETTER_LENGTH};
        char lbl[2]={(char)('A'+i),0};
        b->label=strdup(lbl); b->on_click=fmc_on_letter;
        b->color_normal={40,40,40,255}; b->color_hover={60,60,110,255};
    }

    // ===== 数字键 (圆形, 3列×4行, 从x=89,y=762, 间距62) =====
    const char* nl[]={"1","2","3","4","5","6","7","8","9",".","0","+/-"};
    for (int i=0;i<12;i++) {
        FMCButton* b=&fmc_buttons[FMC_KEY_1+i];
        b->key=(FMCKey)(FMC_KEY_1+i); b->shape=FMC_SHAPE_CIRCLE;
        int cx=89+(i%3)*62, cy=762+(i/3)*62;
        b->rect={cx-NUMB_R, cy-NUMB_R, NUMB_R*2, NUMB_R*2};
        b->radius=NUMB_R; b->label=nl[i]; b->on_click=fmc_on_number;
        b->color_normal={40,40,40,255}; b->color_hover={60,60,110,255};
    }

    // ===== 编辑键 (x=333,401,468,534, y=924) =====
    struct { FMCKey k; int x; const char* l; } ekeys[]={
        {FMC_KEY_SP,    333, "SP"},
        {FMC_KEY_DEL,   401, "DEL"},
        {FMC_KEY_SLASH, 468, "/"},
        {FMC_KEY_CLR,   534, "CLR"},
    };
    for (auto& ek : ekeys) {
        FMCButton* b=&fmc_buttons[ek.k];
        b->key=ek.k; b->shape=FMC_SHAPE_RECT;
        b->rect={ek.x, 924, LETTER_LENGTH, LETTER_LENGTH};
        b->label=ek.l; b->on_click=fmc_on_edit;
        b->color_normal={40,40,40,255}; b->color_hover={60,60,110,255};
    }
}
