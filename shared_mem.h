/**
 * shared_mem.h — FMC ↔ ND 共享内存通信协议
 *
 * ========== 设计原理 ==========
 * 通过 Windows 文件映射 (File Mapping) 实现进程间零拷贝数据共享:
 *   1. FMC 端: 飞行员编辑航路 → write_route() 写入共享内存
 *   2. ND 端:  每帧 has_new_data() 检测版本号变化 → read_route() 读取
 *   3. 飞机位置: update_aircraft_position() 双向同步
 *
 * ========== 同步机制 ==========
 *   写保护:  Mutex互斥锁 (100ms超时) + write_flag原子标志双重保护
 *   版本号:  每次写入 InterlockedIncrement(&version), 读取端通过版本号
 *            判断数据是否更新, 避免重复渲染
 *   飞机位置: 轻量级更新(仅3个double), 使用Mutex保护
 *
 * ========== 内存布局 ==========
 *   SharedRouteData (4096字节):
 *     [0-3]    write_flag  (volatile LONG, 原子标志)
 *     [4-7]    version     (volatile LONG, 版本号)
 *     [8-11]   wpt_count
 *     [12-19]  origin[8]
 *     [20-27]  dest[8]
 *     [28-43]  flt_no[16]
 *     [44-67]  ac_lat/ac_lon/ac_hdg (3×8=24字节)
 *     [68-...] wpts[50]   (50×40=2000字节)
 */

#pragma once
#include <windows.h>
#include <cstring>
#include <cstdio>

// ============================================================
// 共享内存配置常量
// ============================================================
#define SHM_NAME          "Global\\ND_FMC_Route_Shared"  // 内核对象名称 (Global\\前缀跨Session)
#define SHM_MUTEX_NAME    "Global\\ND_FMC_Route_Mutex"   // 互斥锁名称
#define SHM_MAX_WPTS      50          // 最大航路点数量 (Boeing 737典型航线<30点)
#define SHM_NAME_MAX      16          // 航路点名称最大长度 (如 "KSEA", "VOR116.80")
#define SHM_BUF_SIZE      4096        // 共享内存总大小 (一页内存, 对齐友好)

// ============================================================
// 共享内存数据结构
// ============================================================

/** 单个共享航路点 (与FMC内部 RouteWpt 对应, 但使用定长char数组便于IPC) */
struct SharedRoutePoint {
    char   id[SHM_NAME_MAX];   // 航路点标识符 (如 "KSEA", "BLI")
    double lat;                 // 纬度 (deg, WGS84)
    double lon;                 // 经度 (deg, WGS84)
    int    freq;                // 导航频率 (kHz×100, 仅VOR/NDB有效)
    char   type;                // 类型: 'V'=VOR, 'N'=NDB, 'F'=FIX, 'A'=AIRPORT
};

/** 共享内存完整数据结构 (单次读写的基本单元) */
struct SharedRouteData {
    volatile LONG write_flag;   // 写保护标志: 0=空闲可读, 1=FMC正在写入
    volatile LONG version;      // 单调递增版本号 (ND端据此判断数据是否更新)
    int           wpt_count;    // 当前有效航路点数量 (0 ≤ wpt_count ≤ SHM_MAX_WPTS)
    char          origin[8];    // 起飞机场ICAO码 (如 "KSEA")
    char          dest[8];      // 目的地机场ICAO码 (如 "KBFI")
    char          flt_no[16];   // 航班号 (如 "ASA12")
    double        ac_lat;       // 当前飞机纬度 (实时更新)
    double        ac_lon;       // 当前飞机经度 (实时更新)
    double        ac_hdg;       // 当前飞机航向 (deg, 实时更新)
    SharedRoutePoint wpts[SHM_MAX_WPTS];  // 航路点数组 (按航线顺序排列)
};

// ============================================================
// SharedMemoryIPC — 共享内存管理类
// ============================================================
// 封装了Windows文件映射API的创建/读写/释放全生命周期
//
// 使用示例:
//   SharedMemoryIPC shm;
//   shm.create();                           // FMC端创建
//   shm.write_route(wpts, count, ...);       // 写入航路
//   shm.update_aircraft_position(lat,lon,hdg);  // 更新位置
//   shm.read_route(data);                   // ND端读取
//   shm.close();                             // 释放
class SharedMemoryIPC {
public:
    HANDLE              hMapFile = NULL;   // 文件映射内核对象句柄
    HANDLE              hMutex   = NULL;   // 互斥锁句柄
    SharedRouteData*    pData    = nullptr; // 映射视图指针 (直接读写此地址)

    SharedMemoryIPC() = default;

    /**
     * create — 创建或打开共享内存
     *
     * 首个调用进程: 创建新的文件映射 + 初始化数据结构为零
     * 后续调用进程: 打开已存在的文件映射 (GetLastError()==ERROR_ALREADY_EXISTS)
     *
     * @return true=成功, false=失败 (打印错误码到控制台)
     */
    bool create() {
        hMapFile = CreateFileMappingA(
            INVALID_HANDLE_VALUE,       // 使用系统分页文件 (非磁盘文件)
            NULL,                        // 默认安全属性
            PAGE_READWRITE,              // 可读写
            0,                           // 高位大小
            SHM_BUF_SIZE,                // 低位大小
            SHM_NAME                     // 共享内存名称
        );

        if (!hMapFile) {
            printf("[SHM] CreateFileMapping failed: %lu\n", GetLastError());
            return false;
        }

        bool is_new = (GetLastError() != ERROR_ALREADY_EXISTS);

        pData = (SharedRouteData*)MapViewOfFile(
            hMapFile,
            FILE_MAP_ALL_ACCESS,
            0, 0,
            SHM_BUF_SIZE
        );

        if (!pData) {
            printf("[SHM] MapViewOfFile failed: %lu\n", GetLastError());
            CloseHandle(hMapFile);
            hMapFile = NULL;
            return false;
        }

        // 首个进程初始化数据结构
        if (is_new) {
            memset(pData, 0, SHM_BUF_SIZE);
            pData->write_flag = 0;
            pData->version = 0;
            pData->wpt_count = 0;
            printf("[SHM] Created new shared memory (size=%d bytes)\n", SHM_BUF_SIZE);
        } else {
            printf("[SHM] Opened existing shared memory (ver=%ld, wpts=%d)\n",
                   pData->version, pData->wpt_count);
        }

        // 创建互斥锁
        hMutex = CreateMutexA(NULL, FALSE, SHM_MUTEX_NAME);
        if (!hMutex) {
            printf("[SHM] Warning: CreateMutex failed: %lu\n", GetLastError());
        }

        return true;
    }

    // ---- 打开共享内存 (ND端/后续进程调用) ----
    bool open() {
        hMapFile = OpenFileMappingA(
            FILE_MAP_ALL_ACCESS,         // 可读写
            FALSE,                       // 不继承句柄
            SHM_NAME                     // 共享内存名称
        );

        if (!hMapFile) {
            printf("[SHM] OpenFileMapping failed: %lu (FMC not yet started?)\n", GetLastError());
            return false;
        }

        pData = (SharedRouteData*)MapViewOfFile(
            hMapFile,
            FILE_MAP_ALL_ACCESS,
            0, 0,
            SHM_BUF_SIZE
        );

        if (!pData) {
            printf("[SHM] MapViewOfFile failed: %lu\n", GetLastError());
            CloseHandle(hMapFile);
            hMapFile = NULL;
            return false;
        }

        // 打开互斥锁
        hMutex = OpenMutexA(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, SHM_MUTEX_NAME);
        if (!hMutex) {
            printf("[SHM] Warning: OpenMutex failed (no mutex lock): %lu\n", GetLastError());
        }

        printf("[SHM] Opened shared memory (ver=%ld, wpts=%d)\n",
               pData->version, pData->wpt_count);
        return true;
    }

    // ---- FMC写入航路数据 (带互斥锁保护) ----
    bool write_route(const SharedRoutePoint* wpts, int count,
                     const char* origin, const char* dest, const char* flt_no) {
        if (!pData) return false;

        // 获取互斥锁
        DWORD wait_result = WAIT_OBJECT_0;
        if (hMutex) {
            wait_result = WaitForSingleObject(hMutex, 100);  // 100ms超时
        }

        if (wait_result == WAIT_OBJECT_0 || wait_result == WAIT_ABANDONED) {
            // 设置写入标志
            InterlockedExchange(&pData->write_flag, 1);

            // 写入数据
            pData->wpt_count = (count > SHM_MAX_WPTS) ? SHM_MAX_WPTS : count;
            strncpy(pData->origin, origin ? origin : "", 7);
            pData->origin[7] = '\0';
            strncpy(pData->dest, dest ? dest : "", 7);
            pData->dest[7] = '\0';
            strncpy(pData->flt_no, flt_no ? flt_no : "", 15);
            pData->flt_no[15] = '\0';

            for (int i = 0; i < pData->wpt_count; i++) {
                strncpy(pData->wpts[i].id, wpts[i].id, SHM_NAME_MAX - 1);
                pData->wpts[i].id[SHM_NAME_MAX - 1] = '\0';
                pData->wpts[i].lat  = wpts[i].lat;
                pData->wpts[i].lon  = wpts[i].lon;
                pData->wpts[i].freq = wpts[i].freq;
                pData->wpts[i].type = wpts[i].type;
            }

            // 递增版本号
            InterlockedIncrement(&pData->version);

            // 清除写入标志
            InterlockedExchange(&pData->write_flag, 0);

            if (hMutex) ReleaseMutex(hMutex);

            printf("[SHM] Wrote route: %d wpts, version=%ld\n",
                   pData->wpt_count, pData->version);
            return true;
        }

        printf("[SHM] Mutex timeout, write failed\n");
        return false;
    }

    // ---- FMC更新飞机位置 ----
    bool update_aircraft_position(double lat, double lon, double hdg) {
        if (!pData) return false;

        if (hMutex) WaitForSingleObject(hMutex, 50);

        pData->ac_lat = lat;
        pData->ac_lon = lon;
        pData->ac_hdg = hdg;

        if (hMutex) ReleaseMutex(hMutex);
        return true;
    }

    // ---- ND读取航路数据 ----
    bool read_route(SharedRouteData& out) {
        if (!pData) return false;

        // 检查写入标志 (非阻塞读取)
        if (InterlockedCompareExchange(&pData->write_flag, 0, 0) != 0) {
            return false;  // FMC正在写入, 跳过本帧
        }

        // 直接内存拷贝 (version一致性由外部检查)
        memcpy(&out, (const void*)pData, sizeof(SharedRouteData));

        // 清除可能的写入标志残留
        out.write_flag = 0;

        return out.wpt_count > 0;
    }

    // ---- 检查是否有新数据 ----
    bool has_new_data(LONG& last_version) const {
        if (!pData) return false;
        LONG cur = pData->version;
        if (cur != last_version) {
            last_version = cur;
            return true;
        }
        return false;
    }

    // ---- 释放资源 ----
    void close() {
        if (pData) {
            UnmapViewOfFile(pData);
            pData = nullptr;
        }
        if (hMapFile) {
            CloseHandle(hMapFile);
            hMapFile = NULL;
        }
        if (hMutex) {
            CloseHandle(hMutex);
            hMutex = NULL;
        }
    }

    ~SharedMemoryIPC() { close(); }
};
