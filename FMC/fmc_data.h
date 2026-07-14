/**
 * fmc_data.h — 进离场数据结构与查询接口
 */
#pragma once

// 元素类型枚举
typedef enum {
    TYPE_RUNWAY,       // 跑道
    TYPE_TAKEOFF_PROC, // 离场/进场程序
    TYPE_WAYPOINT      // 过渡点
} DepArrElemType;

// 基础元素结构体
typedef struct {
    char name[20];
    DepArrElemType type;
    char airport[20];
} DepArrElement;

// 关联关系结构体
typedef struct {
    char airport[20];
    char elem1[20];
    DepArrElemType elem1_type;
    char elem2[20];
    DepArrElemType elem2_type;
} DepArrRelation;

// 用户选择状态
typedef struct {
    char select_runway[20];
    char select_proc[20];
    char select_runway_trans[20];
    char select_proc_trans[20];
    int  select_flag;  // 0=SEL(未执行), 1=ACT(已执行)
} SelectDepArr;

// 全局数据
extern DepArrElement g_deparr_elements[200];
extern DepArrRelation g_deparr_relations[2000];
extern int            g_deparr_element_count;
extern int            g_deparr_relation_count;

// 展示列表
extern char** runway;
extern char** proc;
extern char** runway_trans;
extern char** proc_trans;
extern int    runway_count;
extern int    proc_count;
extern int    runway_trans_count;
extern int    proc_trans_count;

// DEP/ARR 页面状态
extern SelectDepArr select_dep_arr[2];  // [0]=离场, [1]=进场
extern int          dep_arr_index;       // 当前分页索引(1-based)
extern int          dep_arr_type;        // 0=起始离场, 1=起始进场, 2=终点进场
extern char         show_ariport[20];    // 当前显示机场ICAO
extern int          will_exec;           // EXEC灯标志

// 数据操作
void add_element(const char* name, DepArrElemType type, const char* airport);
void add_relation(const char* airport, const char* elem1, DepArrElemType el1_type,
                  const char* elem2, DepArrElemType el2_type);
void init_airport_data(void);
void destroy_airport_data(void);

// 查询函数
int query_runway_proc_by_airport(const char* airport);
int query_proc_by_runway(const char* airport, const char* runway);
int query_trans_by_runway(const char* airport, const char* runway);
int query_runway_by_proc(const char* airport, const char* proc);
int query_trans_by_proc(const char* airport, const char* proc);
