// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright 2026 Netto Project Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// ==== DESCRIPTION ===========================================================

/**
 * @file    shm_logger.c
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-05-15
 * @brief   공유 메모리 로깅 유틸리티 소스 파일
 */

// ==== INCLUDES ==============================================================

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <immintrin.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <linux/futex.h>

#include "shm_logger.h"
#include "config.h"
#include "nt_time.h"

// ==== DEFINES / MACROS ======================================================

#define DEF_MAX_LOG_SIZE    (10 * 1024 * 1024)	// 10MB
#define DEF_MAX_BACKUP      5

#define RING_BUFFER_MAGIC   0xA5FB06A0U

#define DAEMON_SLOT_COUNT               1024
#define DAEMON_SPIN_MAX                 1024
#define DAEMON_YIELD_MAX                2048
#define DAEMON_SLOT_CRASH_TIMEOUT_NS    (100 * 1000 * 1000) // 100ms

#define DAEMON_SHM_PATH     "/daemon_log_shm"

// ==== TYPEDEFS / STRUCTS ====================================================

// 슬롯 상태 (crash-safe FSM)
typedef enum slot_status {
    SLOT_EMPTY = 0, // 슬롯이 비어있음
    SLOT_READY,     // Consumer가 읽을 수 있음
} slot_status_t;

// Daemon 로그 슬롯 구조체
typedef struct daemon_log_slot {
    uint64_t timestamp_ns;
    atomic_uint state;
    uint32_t pid;
    uint32_t level;
    char msg[BUF_SIZE_1K];
} __attribute__((aligned(64))) daemon_log_slot_t;

// Daemon 로그 링 버퍼 구조체
typedef struct daemon_ring_buffer {
    __attribute__((aligned(64))) atomic_uint_fast64_t write_seq;

    __attribute__((aligned(64))) atomic_uint_fast64_t read_seq;

    __attribute__((aligned(64))) atomic_uint consumer_sleep_flag;

    struct {
        uint32_t magic;
    } __attribute__((aligned(64))) meta;

    __attribute__((aligned(64))) daemon_log_slot_t slots[DAEMON_SLOT_COUNT];
} daemon_ring_buffer_t;

// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================

static daemon_ring_buffer_t *g_daemon_rb = NULL;

static int g_shm_fd[LOG_TYPE_MAX] = {-1};
static char *g_shm_path[LOG_TYPE_MAX] = {
    [LOG_TYPE_DAEMON] = DAEMON_SHM_PATH
};

// ==== FUNCTION PROTOTYPES ===================================================

static void *init_consumer_shm(log_type_t type);
static void *init_producer_shm(log_type_t type);
static size_t get_shm_size(log_type_t type);
static inline int wait_futex_timeout(atomic_uint *addr, uint32_t expected, uint64_t timeout_ns);
static inline void wake_futex(atomic_uint *addr, int n);

// ==== FUNCTIONS =============================================================

int init_daemon_logger_consumer(void)
{
    int i = 0;

    g_daemon_rb = (daemon_ring_buffer_t *)init_consumer_shm(LOG_TYPE_DAEMON);
    if (g_daemon_rb == NULL) {
        return -1;
    }

    atomic_init(&g_daemon_rb->write_seq, 0);
    atomic_init(&g_daemon_rb->read_seq, 0);
    atomic_init(&g_daemon_rb->consumer_sleep_flag, 0);

    for (i = 0; i < DAEMON_SLOT_COUNT; i++) {
        atomic_init(&g_daemon_rb->slots[i].state, SLOT_EMPTY);
    }

    // 모든 슬롯 초기화 완료 후 magic 세팅 → 프로듀서의 acquire fence와 동기화
    atomic_thread_fence(memory_order_release);
    g_daemon_rb->meta.magic = RING_BUFFER_MAGIC;

    return 0;
}

int init_daemon_logger_producer(void)
{
    g_daemon_rb = (daemon_ring_buffer_t *)init_producer_shm(LOG_TYPE_DAEMON);
    if (g_daemon_rb == NULL) {
        return -1;
    }

    // 메타데이터가 완전히 초기화된 것을 보장하기 위해 메모리 배리어 적용
    atomic_thread_fence(memory_order_acquire);
    if (g_daemon_rb->meta.magic != RING_BUFFER_MAGIC) {
        destroy_logger(LOG_TYPE_DAEMON, false);
        return -1;
    }

    return 0;
}

void destroy_logger(log_type_t type, bool is_consumer)
{
    if (type >= LOG_TYPE_MAX) {
        return;
    }

    switch (type) {
        case LOG_TYPE_DAEMON:
            if (g_daemon_rb != NULL) {
                munmap(g_daemon_rb, sizeof(daemon_ring_buffer_t));
                g_daemon_rb = NULL;
            }
            break;
        default:
            break;
    }

    CLOSE_FD(g_shm_fd[type]);

    if (is_consumer) {
        shm_unlink(g_shm_path[type]);
    }
}

void write_daemon_log(log_level_t level, const char *fmt, ...)
{
    va_list args;
    daemon_log_slot_t *slot = NULL;
    uint64_t write_seq = 0, read_seq = 0;
    int msg_len = 0;
    char msg[BUF_SIZE_1K] = {0};

    if (g_daemon_rb == NULL || level >= LOG_LVL_MAX || fmt == NULL) {
        return;
    }

    va_start(args, fmt);
    msg_len = vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    if (msg_len < 0) {
        return;
    }

    uint64_t now_ns = get_now_time_ns();
    uint32_t pid = (uint32_t)getpid();

    while (true) {
        write_seq = atomic_load_explicit(&g_daemon_rb->write_seq, memory_order_relaxed);
        read_seq = atomic_load_explicit(&g_daemon_rb->read_seq, memory_order_acquire);

        // 버퍼가 가득 찼는지 확인
        if (write_seq - read_seq >= DAEMON_SLOT_COUNT) {
            break;
        }

        // 원자적으로 write 시퀀스를 증가시켜 슬롯 예약
        if (!atomic_compare_exchange_weak_explicit(&g_daemon_rb->write_seq, 
            &write_seq, write_seq + 1, memory_order_release, memory_order_relaxed)) 
        {
            continue; // 다른 producer가 시퀀스를 변경했으므로 재시도
        }

        // 슬롯에 로그 데이터 작성
        slot = &g_daemon_rb->slots[write_seq % DAEMON_SLOT_COUNT];
        slot->timestamp_ns = now_ns;
        slot->pid = pid;
        slot->level = level;
        strncpy(slot->msg, msg, sizeof(slot->msg) - 1);

        // 슬롯 상태를 READY로 설정하여 consumer가 읽을 수 있도록 함
        atomic_store_explicit(&slot->state, SLOT_READY, memory_order_release);

        // Consumer가 대기 중이면 깨움
        if (atomic_load_explicit(&g_daemon_rb->consumer_sleep_flag, memory_order_acquire) == 1) {
            atomic_store_explicit(&g_daemon_rb->consumer_sleep_flag, 0, memory_order_release);
            wake_futex(&g_daemon_rb->consumer_sleep_flag, 1);
        }
        break;
    }
}

void *daemon_log_consumer_thread(void *arg)
{
    daemon_log_slot_t *slot = NULL;
    uint64_t read_seq = 0, write_seq = 0;
    uint64_t empty_count = 0, crash_time_ns = 0;

    while (true) {
        read_seq = atomic_load_explicit(&g_daemon_rb->read_seq, memory_order_relaxed);
        write_seq = atomic_load_explicit(&g_daemon_rb->write_seq, memory_order_acquire);

        // 종료 신호가 왔고 읽을 로그가 없으면 종료
        if (!g_config.running && read_seq >= write_seq) {
            break;
        }

        slot = &g_daemon_rb->slots[read_seq % DAEMON_SLOT_COUNT];

        // 해당 슬롯이 READY 상태인지 확인
        if (atomic_load_explicit(&slot->state, memory_order_acquire) == SLOT_READY) {
            // TODO: 로그 처리 (예: 파일에 쓰기)
            // process_log(slot);

            // 슬롯 상태를 EMPTY로 설정
            atomic_store_explicit(&slot->state, SLOT_EMPTY, memory_order_release);
            // 읽은 시퀀스를 증가시켜 다음 슬롯으로 이동
            atomic_fetch_add_explicit(&g_daemon_rb->read_seq, 1, memory_order_release);

            empty_count = 0;
            crash_time_ns = 0;
            continue;
        }

        // 슬롯을 선점한 producer가 있을 때, 일정 시간 동안 READY 상태로 
        // 전환되지 않으면 producer가 크래시된 것으로 간주하고 슬롯을 비움
        if (read_seq < write_seq) {
            if (crash_time_ns == 0) {
                // 크래시 타이머 시작
                crash_time_ns = get_now_time_ns();
            }
            else if (get_now_time_ns() - crash_time_ns > DAEMON_SLOT_CRASH_TIMEOUT_NS) {
                // 슬롯이 일정 시간 동안 READY 상태로 전환되지 않음 -> producer 크래시로 간주하고 슬롯을 비움
                atomic_store_explicit(&slot->state, SLOT_EMPTY, memory_order_release);
                atomic_fetch_add_explicit(&g_daemon_rb->read_seq, 1, memory_order_release);

                empty_count = 0;
                crash_time_ns = 0;
                continue;
            }
        }
        else {
            // 아직 작성되지 않은 슬롯이므로 타이머 리셋
            crash_time_ns = 0;
        }

        if (empty_count < DAEMON_SPIN_MAX) {
            // CPU를 잠시 쉬게 하여 다른 스레드가 실행될 수 있도록 함
            _mm_pause();
        }
        else if (empty_count < DAEMON_YIELD_MAX) {
            // 일정 횟수 이상 빈 슬롯이 계속되면 스케줄러에 양보하여 다른 스레드가 실행될 수 있도록 함
            sched_yield();
        }
        else {
            // 일정 횟수 이상 빈 슬롯이 계속되면 Futex로 대기하여 CPU 자원을 낭비하지 않도록 함
            atomic_store_explicit(&g_daemon_rb->consumer_sleep_flag, 1, memory_order_release);

            // Double-checking: 슬롯이 READY 상태로 전환되었는지 다시 확인하여 스피닝과 Futex 대기 사이의 경합을 줄임
            if (atomic_load_explicit(&slot->state, memory_order_acquire) != SLOT_READY) {
                // 슬롯이 아직 READY 상태가 아니므로 Futex로 대기
                // 크래시 타이머가 이미 시작된 경우, 타이머가 만료될 때까지 대기 시간을 
                // 1ms로 제한하여 크래시 감지 기능이 계속 작동하도록 함
                wait_futex_timeout(&g_daemon_rb->consumer_sleep_flag, 1, 
                    (crash_time_ns) ? 1000000ULL : 100000000ULL);
            }

            atomic_store_explicit(&g_daemon_rb->consumer_sleep_flag, 0, memory_order_release);
        }

        empty_count++;
    }

    return NULL;
}

/**
 * @brief Consumer 초기화 - 공유 메모리 생성 및 매핑
 * 
 * @param type 로그 타입
 * @return void * 매핑된 공유 메모리 포인터, 실패 시 NULL
 */
static void *init_consumer_shm(log_type_t type)
{
    size_t shm_size = 0;
    void *ptr = NULL;
    int ret = -1;

    shm_unlink(g_shm_path[type]);

    g_shm_fd[type] = shm_open(g_shm_path[type], O_CREAT | O_EXCL | O_RDWR, 0666);
    if (g_shm_fd[type] < 0) {
        return NULL;
    }

    shm_size = get_shm_size(type);

    do {
        if (ftruncate(g_shm_fd[type], shm_size) < 0) {
            break;
        }

        ptr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, g_shm_fd[type], 0);
        if (ptr == MAP_FAILED) {
            break;
        }

        memset(ptr, 0, shm_size);

        ret = 0;
    } while (0);

    if (ret != 0) {
        CLOSE_FD(g_shm_fd[type]);
        shm_unlink(g_shm_path[type]);
        return NULL;
    }

    return ptr;
}

/**
 * @brief Producer 초기화 - 공유 메모리 매핑
 * 
 * @param type 로그 타입
 * @return void * 매핑된 공유 메모리 포인터, 실패 시 NULL
 */
static void *init_producer_shm(log_type_t type)
{
    size_t shm_size = 0;
    void *ptr = NULL;

    g_shm_fd[type] = shm_open(g_shm_path[type], O_RDWR, 0666);
    if (g_shm_fd[type] < 0) {
        return NULL;
    }

    shm_size = get_shm_size(type);

    ptr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, g_shm_fd[type], 0);
    if (ptr == MAP_FAILED) {
        CLOSE_FD(g_shm_fd[type]);
        return NULL;
    }

    return ptr;
}

/**
 * @brief 공유 메모리 크기 계산 - 로그 타입별로 필요한 크기를 반환
 * 
 * @param type 로그 타입
 * @return size_t 공유 메모리 크기, 지원하지 않는 타입은 0 반환
 */
static size_t get_shm_size(log_type_t type)
{
    switch (type) {
        case LOG_TYPE_DAEMON:
            return sizeof(daemon_ring_buffer_t);
        default:
            return 0;
    }
}

/**
 * @brief Futex 대기 - 타임아웃을 포함한 Futex 대기 함수
 * 
 * @param addr Futex 주소
 * @param expected 기대 값
 * @param timeout_ns 타임아웃 시간 (나노초)
 * @return int 시스템 호출 결과
 */
static inline int wait_futex_timeout(atomic_uint *addr, uint32_t expected, uint64_t timeout_ns)
{
    struct timespec ts = {
        .tv_sec = timeout_ns / 1000000000,
        .tv_nsec = timeout_ns % 1000000000
    };

    return syscall(SYS_futex, (uint32_t *)addr, FUTEX_WAIT, expected, &ts, NULL, 0);
}

/**
 * @brief Futex 깨우기 - 지정된 수의 스레드를 깨우는 함수
 * 
 * @param addr Futex 주소
 * @param n 깨울 스레드 수
 */
static inline void wake_futex(atomic_uint *addr, int n)
{
    syscall(SYS_futex, (uint32_t *)addr, FUTEX_WAKE, n, NULL, NULL, 0);
}
