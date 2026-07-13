/**
 * fmc_route.h — 航路数据管理 (AVL平衡二叉树 + RouteArray航道数组)
 *
 * ========== 数据结构 ==========
 *
 * AVLTree (自平衡二叉搜索树):
 *   用途: 全局航路点数据库的快速查询 (O(log N) 精确搜索 + 前缀搜索)
 *   存储: 从 earth_nav.dat / earth_fix.dat / apt.dat 加载的所有导航点
 *   操作: insert(插入) / search(精确查询) / search_prefix(前缀模糊搜索)
 *
 * RouteArray (航道数组):
 *   用途: 当前飞行计划的航路段管理
 *   存储: RouteWpt[] (航路点坐标) + RouteLeg[] (航段via/to信息)
 *   翻页: 第0页=输入表单(ORIGIN/DEST/FLT_NO), 第1+页=每页4个航段
 *
 * ========== AVL树旋转操作 ==========
 *   LL (左左): 右旋 → 解决左子树过高
 *   RR (右右): 左旋 → 解决右子树过高
 *   LR (左右): 左子左旋 + 自身右旋
 *   RL (右左): 右子右旋 + 自身左旋
 *
 * ========== 知识点 ==========
 *   - AVL树: 自平衡二叉搜索树, |平衡因子|≤1, 查找O(log N)
 *   - 中序遍历前缀搜索: 利用BST有序性进行范围查询
 *   - 翻页分段显示: RTE页面按LEGS_PER_PAGE=4分段展示航段
 */

#pragma once
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <vector>

// ============================================================
// RouteWpt — 单个航路点
// ============================================================
// 统一定义航路点的所有属性, 包括导航台类型和频率
struct RouteWpt {
    char id[16];        // 航路点标识符 (如 "KSEA", "BLI", "VOR116.8")
    double lat, lon;    // 地理坐标 (WGS84)
    int    freq;        // 导航频率 (kHz×100, 如 11680=116.80MHz)
    char   type;        // 类型: 'V'=VOR, 'N'=NDB, 'F'=FIX, 'A'=AIRPORT
};

// ============================================================
// AVLNode — AVL平衡二叉树节点
// ============================================================
// 每个节点存储一个航路点 + 左右子树指针 + 高度值
// height 用于计算平衡因子: balance = height(left) - height(right)
struct AVLNode {
    RouteWpt  wpt;          // 节点存储的航路点数据
    AVLNode*  left;         // 左子树 (ID字典序更小的节点)
    AVLNode*  right;        // 右子树 (ID字典序更大的节点)
    int       height;       // 以该节点为根的子树高度

    AVLNode(const RouteWpt& w) : wpt(w), left(nullptr), right(nullptr), height(1) {}
};

// ============================================================
// AVLTree — 自平衡二叉搜索树
// ============================================================
// 用于航路点数据库的快速查询, 支持:
//   - 精确ID查询 search(id)       O(log N)
//   - 前缀模糊搜索 search_prefix()  O(N) 中序遍历
//   - 插入自动平衡 insert()         O(log N) 含旋转调整
struct AVLTree {
    AVLNode* root;   // 根节点
    int      count;  // 节点总数

    AVLTree() : root(nullptr), count(0) {}

    /** 获取节点高度 (空节点高度=0) */
    int height(AVLNode* n) { return n ? n->height : 0; }

    /** 计算平衡因子: 正值=左重, 负值=右重, 0=平衡 */
    int balance(AVLNode* n) { return n ? height(n->left) - height(n->right) : 0; }

    AVLNode* rotate_right(AVLNode* y) {
        AVLNode* x = y->left;
        AVLNode* T = x->right;
        x->right = y; y->left = T;
        y->height = 1 + (height(y->left) > height(y->right) ? height(y->left) : height(y->right));
        x->height = 1 + (height(x->left) > height(x->right) ? height(x->left) : height(x->right));
        return x;
    }

    AVLNode* rotate_left(AVLNode* x) {
        AVLNode* y = x->right;
        AVLNode* T = y->left;
        y->left = x; x->right = T;
        x->height = 1 + (height(x->left) > height(x->right) ? height(x->left) : height(x->right));
        y->height = 1 + (height(y->left) > height(y->right) ? height(y->left) : height(y->right));
        return y;
    }

    AVLNode* insert_node(AVLNode* node, const RouteWpt& wpt) {
        if (!node) { count++; return new AVLNode(wpt); }
        int cmp = strcmp(wpt.id, node->wpt.id);
        if (cmp < 0)
            node->left = insert_node(node->left, wpt);
        else if (cmp > 0)
            node->right = insert_node(node->right, wpt);
        else
            return node; // 重复

        node->height = 1 + (height(node->left) > height(node->right) ? height(node->left) : height(node->right));
        int bal = balance(node);

        // LL
        if (bal > 1 && strcmp(wpt.id, node->left->wpt.id) < 0) return rotate_right(node);
        // RR
        if (bal < -1 && strcmp(wpt.id, node->right->wpt.id) > 0) return rotate_left(node);
        // LR
        if (bal > 1 && strcmp(wpt.id, node->left->wpt.id) > 0) { node->left = rotate_left(node->left); return rotate_right(node); }
        // RL
        if (bal < -1 && strcmp(wpt.id, node->right->wpt.id) < 0) { node->right = rotate_right(node->right); return rotate_left(node); }
        return node;
    }

    void insert(const RouteWpt& wpt) { root = insert_node(root, wpt); }

    // 按ID查询
    AVLNode* search_node(AVLNode* node, const char* id) {
        if (!node) return nullptr;
        int cmp = strcmp(id, node->wpt.id);
        if (cmp == 0) return node;
        if (cmp < 0) return search_node(node->left, id);
        return search_node(node->right, id);
    }
    AVLNode* search(const char* id) { return search_node(root, id); }

    // 前缀搜索 (返回匹配前缀的所有节点)
    void search_prefix(AVLNode* node, const char* prefix, std::vector<RouteWpt>& results, int limit=50) {
        if (!node || (int)results.size() >= limit) return;
        search_prefix(node->left, prefix, results, limit);
        if ((int)results.size() >= limit) return;
        if (strncmp(node->wpt.id, prefix, strlen(prefix)) == 0)
            results.push_back(node->wpt);
        search_prefix(node->right, prefix, results, limit);
    }
    std::vector<RouteWpt> search_prefix(const char* prefix) {
        std::vector<RouteWpt> results;
        search_prefix(root, prefix, results);
        return results;
    }

    void destroy_node(AVLNode* n) { if (n) { destroy_node(n->left); destroy_node(n->right); delete n; } }
    ~AVLTree() { destroy_node(root); }
};

// ============================================================
// RouteLeg — 单个航段
// ============================================================
// 飞行计划中两个航路点之间的航段, 包含航路名称和目的航路点
struct RouteLeg {
    char via[16];      // 航路名称: "DIRECT"=直飞 或 高空航路编号如 "J523"
    char to[16];       // 到达航路点ID (如 "BLI", "FREDY")
    double to_lat, to_lon;  // 到达点地理坐标
};

// ============================================================
// RouteArray — 航道数组 (飞行计划管理)
// ============================================================
// 管理当前飞行计划的所有航路点和航段,
// 支持分页浏览: 第0页=输入表单(ORIGIN/DEST/FLT_NO), 第1+页=航段列表
constexpr int MAX_ROUTE_WPTS = 50;   // 最大航路点容量
constexpr int LEGS_PER_PAGE  = 4;    // RTE页面每页显示的航段数

struct RouteArray {
    RouteWpt wpts[MAX_ROUTE_WPTS];
    RouteLeg legs[MAX_ROUTE_WPTS];
    int      count;
    int      current_page;      // 当前显示页 (0-based)

    RouteArray() : count(0), current_page(0) {}

    void clear() { count = 0; current_page = 0; memset(legs, 0, sizeof(legs)); }
    bool is_full() const { return count >= MAX_ROUTE_WPTS; }

    // 添加航段 (默认DIRECT)
    bool append_leg(const char* wpt_id, double lat, double lon) {
        if (count >= MAX_ROUTE_WPTS) return false;
        strncpy(legs[count].via, "DIRECT", 15);
        strncpy(legs[count].to, wpt_id, 15);
        legs[count].to_lat = lat;
        legs[count].to_lon = lon;
        // 同步到 wpts 数组 (兼容旧逻辑)
        strncpy(wpts[count].id, wpt_id, 15);
        wpts[count].lat = lat;
        wpts[count].lon = lon;
        wpts[count].type = 'F';
        count++;
        return true;
    }

    // 翻页 (page 0=输入表单, page 1+=航段)
    int total_pages() const { return 1 + (count + LEGS_PER_PAGE - 1) / LEGS_PER_PAGE; }
    int page_start() const { return (current_page - 1) * LEGS_PER_PAGE; }
    int page_end()   const { int e = page_start() + LEGS_PER_PAGE; return e < count ? e : count; }

    bool can_next() const { return current_page < total_pages() - 1; }
    bool can_prev() const { return current_page > 0; }

    void next_page() { if (can_next()) current_page++; }
    void prev_page() { if (can_prev()) current_page--; }
    const RouteWpt* get_page_wpt(int idx) const {
        int actual = page_start() + idx;
        if (actual >= count) return nullptr;
        return &wpts[actual];
    }
};

// ===== 航道数据加载 =====
extern AVLTree   g_waypoint_tree;   // 全局航路点AVL树
extern RouteArray g_route;          // 当前航道数组

// 从导航数据文件加载航路点到AVL树
void load_navdata_avl(const char* nav_path, const char* fix_path, const char* apt_path);

// 批量添加航路点 (从KSEAKBFI.fms)
void load_route_from_fms(const char* path);
