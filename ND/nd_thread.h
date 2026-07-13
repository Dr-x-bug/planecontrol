/**
 * nd_thread.h — ND 数据采集子线程 (双缓冲无锁同步)
 *
 * ========== 架构 ==========
 * 使用 Windows CreateThread 创建独立的数据采集线程,
 * 循环从 X-Plane 获取飞行数据并通过双缓冲 (double-buffering)
 * 与主渲染线程同步, 避免数据竞争。
 *
 * ========== 双缓冲同步 ==========
 * 原理:
 *   SharedNDBuffer 包含两个 NDFlightData 缓冲区 (buf[0], buf[1]),
 *   子线程写入 write_idx 指向的缓冲区, 写入完成后原子交换索引;
 *   主线程从 read_idx 读取最新完成的数据帧。
 *
 * 同步原语:
 *   atomic_swap_buffer()  — InterlockedExchange 原子交换读写索引
 *   atomic_read_data()    — InterlockedCompareExchange 原子读取
 *
 * 优点:
 *   - 无锁设计: 不需要Mutex/CriticalSection, 性能极高
 *   - 读写分离: 写入和读取操作不同的缓冲区, 永不冲突
 *   - 数据完整性: 主线程始终读到完整的一帧数据
 *
 * ========== 知识点 ==========
 *   - Windows多线程: CreateThread / WaitForSingleObject / TerminateThread
 *   - 原子操作: InterlockedExchange / InterlockedCompareExchange / InterlockedIncrement
 *   - 双缓冲: 生产者-消费者模型的无锁实现
 */

#pragma once
#include <windows.h>
#include <cstdio>
#include <cstring>
#include "nd_xpc.h"

// ===== 双缓冲共享数据结构 =====
// 数据子线程写入 write_buf，主线程读取 read_buf
// 通过 atomic_flag 实现无锁同步

struct SharedNDBuffer {
    NDFlightData buf[2];          // 双缓冲
    volatile LONG write_idx;      // 写入索引 (原子更新)
    volatile LONG read_idx;       // 读取索引
    volatile LONG data_ready;     // 数据就绪标志 (0/1)
    volatile LONG running;        // 线程运行控制 (1=运行, 0=退出)
    volatile LONG frame_count;    // 数据帧计数
};

// 全局共享缓冲区
extern SharedNDBuffer g_shared;

// 原子交换读写索引 (数据子线程调用)
inline void atomic_swap_buffer() {
    LONG w = g_shared.write_idx;
    g_shared.read_idx  = w;
    g_shared.write_idx = 1 - w;
    InterlockedExchange(&g_shared.data_ready, 1);
    InterlockedIncrement(&g_shared.frame_count);
}

// 原子读取最新数据 (主线程调用)
inline bool atomic_read_data(NDFlightData& out) {
    if (InterlockedCompareExchange(&g_shared.data_ready, 0, 1) == 1) {
        LONG r = g_shared.read_idx;
        memcpy(&out, &g_shared.buf[r], sizeof(NDFlightData));
        return true;
    }
    return false;
}

// ===== 数据子线程函数 =====
// 职责: 循环从 X-Plane 获取导航数据, 写入共享缓冲区
inline DWORD WINAPI nd_data_thread(LPVOID /*param*/) {
    printf("[Thread] Data thread started (ID=%lu)\n", GetCurrentThreadId());

    // 初始化 XPC 连接
    xpc_init("127.0.0.1", 49009);

    while (InterlockedCompareExchange(&g_shared.running, 1, 1) == 1) {
        // 从 X-Plane 获取数据
        bool ok = nd_fetch_all();

        if (ok) {
            // 写入当前缓冲区
            LONG w = g_shared.write_idx;
            g_shared.buf[w] = g_nd_data;

            // 原子交换双缓冲
            atomic_swap_buffer();
        }

        // 控制更新频率 (~10Hz)
        Sleep(100);
    }

    xpc_close();
    printf("[Thread] Data thread exited\n");
    return 0;
}

// ===== 线程管理 =====

// 创建数据子线程
inline HANDLE nd_thread_start() {
    // 初始化共享缓冲区
    g_shared.write_idx  = 0;
    g_shared.read_idx   = 0;
    g_shared.data_ready = 0;
    g_shared.running    = 1;
    g_shared.frame_count = 0;

    // CreateThread: Windows 平台多线程创建
    HANDLE hThread = CreateThread(
        NULL,                     // 默认安全属性
        0,                        // 默认栈大小
        nd_data_thread,           // 线程函数
        NULL,                     // 线程参数
        0,                        // 立即运行
        NULL                      // 不需要线程ID
    );

    if (!hThread) {
        printf("[Thread] Failed to create data thread!\n");
    }
    return hThread;
}

// 停止数据子线程并等待退出
inline void nd_thread_stop(HANDLE hThread) {
    if (!hThread) return;

    // 设置退出标志
    InterlockedExchange(&g_shared.running, 0);

    // WaitForSingleObject: 等待线程退出 (最多5秒超时)
    DWORD result = WaitForSingleObject(hThread, 5000);
    if (result == WAIT_OBJECT_0) {
        printf("[Thread] Data thread joined successfully\n");
    } else if (result == WAIT_TIMEOUT) {
        printf("[Thread] Thread join timeout, forcing termination\n");
        TerminateThread(hThread, 0);
    }

    CloseHandle(hThread);
}
