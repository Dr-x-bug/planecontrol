/**
 * shared_mem.h — FMC ↔ ND 共享内存通信协议
 *
 * 通过 Windows 共享内存实现进程间通信(IPC)
 * - FMC 端: 飞行员输入航路, 写入共享内存
 * - ND 端:  读取共享内存航路列表, 在地图上渲染飞行航线
 *
 * 知识点: 进程间通信(IPC)、共享内存机制、数据同步
 * 重点:   共享内存的创建与数据读写
 * 难点:   多进程数据同步与一致性保障
 */

#pragma once
#include <windows.h>
#include <cstring>
#include <cstdio>

// ===== 共享内存配置 =====
#define SHM_NAME          "Global\\ND_FMC_Route_Shared"
#define SHM_MUTEX_NAME    "Global\\ND_FMC_Route_Mutex"
#define SHM_MAX_WPTS      50          // 最大航路点数量
#define SHM_NAME_MAX      16          // 航路点名称最大长度
#define SHM_BUF_SIZE      4096        // 共享内存总大小

// ===== 共享内存数据结构 =====
struct SharedRoutePoint {
    char   id[SHM_NAME_MAX];   // 航路点标识符 (ICAO/VOR/FIX/AIRPORT)
    double lat;                 // 纬度 (deg)
    double lon;                 // 经度 (deg)
    int    freq;                // 频率 (VOR/NDB, kHz*100)
    char   type;                // 'V'=VOR, 'N'=NDB, 'F'=FIX, 'A'=AIRPORT
};

struct SharedRouteData {
    volatile LONG write_flag;   // 0=可读, 1=FMC正在写入 (原子标志)
    volatile LONG version;      // 数据版本号 (每次写入递增)
    int           wpt_count;    // 当前航路点数量
    char          origin[8];    // 起飞机场 (ICAO)
    char          dest[8];      // 目的地机场 (ICAO)
    char          flt_no[16];   // 航班号
    double        ac_lat;       // 当前飞机纬度
    double        ac_lon;       // 当前飞机经度
    double        ac_hdg;       // 当前飞机航向
    SharedRoutePoint wpts[SHM_MAX_WPTS];  // 航路点数组
};

// ===== 共享内存管理类 =====
class SharedMemoryIPC {
public:
    HANDLE              hMapFile = NULL;
    HANDLE              hMutex   = NULL;
    SharedRouteData*    pData    = nullptr;

    SharedMemoryIPC() = default;

    // ---- 创建共享内存 (FMC端/首个进程调用) ----
    bool create() {
        hMapFile = CreateFileMappingA(
            INVALID_HANDLE_VALUE,       // 使用系统分页文件
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
