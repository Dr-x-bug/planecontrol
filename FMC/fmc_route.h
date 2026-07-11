#pragma once
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <vector>

// ===== 航道航路点 =====
struct RouteWpt {
    char id[16];        // 航路点ID
    double lat, lon;    // 经纬度
    int    freq;        // 频率 (VOR/NDB)
    char   type;        // 'V'=VOR, 'N'=NDB, 'F'=FIX, 'A'=AIRPORT
};

// ===== AVL平衡二叉树节点 =====
struct AVLNode {
    RouteWpt  wpt;
    AVLNode*  left;
    AVLNode*  right;
    int       height;

    AVLNode(const RouteWpt& w) : wpt(w), left(nullptr), right(nullptr), height(1) {}
};

// ===== AVL树 (航路点快速查询) =====
struct AVLTree {
    AVLNode* root;
    int      count;

    AVLTree() : root(nullptr), count(0) {}

    int height(AVLNode* n) { return n ? n->height : 0; }
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

// ===== 航段结构 =====
struct RouteLeg {
    char via[16];      // "DIRECT" 或 airway名
    char to[16];       // 航路点ID
    double to_lat, to_lon;
};

// ===== 航道数组 (RTE页面数据) =====
constexpr int MAX_ROUTE_WPTS = 50;
constexpr int LEGS_PER_PAGE  = 4;    // 每页4个航段

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
