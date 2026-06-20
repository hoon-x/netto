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
 * @file    event_count.c
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-06-09
 * @brief   Lock-Free 알고리즘용 조건 변수 구현 소스 파일
 */

// ==== INCLUDES ==============================================================

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <limits.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils/event_count.h"

// ==== DEFINES / MACROS ======================================================

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define EC_EPOCH_OFFSET 1 // epoch word는 상위 32비트 -> LE에서 [4..7]
#else
#define EC_EPOCH_OFFSET 0
#endif

// ==== TYPEDEFS / STRUCTS ====================================================
// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================

static const uint64_t EC_ADD_WAITER  = UINT64_C(1);
static const uint64_t EC_SUB_WAITER  = (uint64_t)-1;    // wrapping sub
static const uint64_t EC_ADD_EPOCH   = UINT64_C(1) << 32;
static const uint64_t EC_WAITER_MASK = (UINT64_C(1) << 32) - 1;

// ==== FUNCTION PROTOTYPES ===================================================

static inline void ec_futex_wait(uint32_t *addr, uint32_t val);
static inline void ec_futex_wake(uint32_t *addr, int n);
static inline uint32_t *ec_epoch_addr(event_count_t *ec);

// ==== FUNCTIONS =============================================================

void ec_init(event_count_t *ec)
{
    atomic_init(&ec->val, 0);
}

void ec_notify(event_count_t *ec)
{
    uint64_t prev = atomic_fetch_add_explicit(&ec->val, EC_ADD_EPOCH, memory_order_acq_rel);
    if (__builtin_expect(!!(prev & EC_WAITER_MASK), 0)) {
        ec_futex_wake(ec_epoch_addr(ec), 1);
    }
}

void ec_notify_all(event_count_t *ec)
{
    uint64_t prev = atomic_fetch_add_explicit(&ec->val, EC_ADD_EPOCH, memory_order_acq_rel);
    if (__builtin_expect(!!(prev & EC_WAITER_MASK), 0)) {
        ec_futex_wake(ec_epoch_addr(ec), INT_MAX);
    }
}

ec_key_t ec_prepare_wait(event_count_t *ec)
{
    uint64_t prev = atomic_fetch_add_explicit(&ec->val, EC_ADD_WAITER, memory_order_acq_rel);
    ec_key_t key = { .epoch = (uint32_t)(prev >> 32) };
    return key;
}

void ec_cancel_wait(event_count_t *ec)
{
    atomic_fetch_add_explicit(&ec->val, EC_SUB_WAITER, memory_order_seq_cst);
}

void ec_wait(event_count_t *ec, ec_key_t key)
{
    while ((uint32_t)(atomic_load_explicit(&ec->val, memory_order_acquire) >> 32) == key.epoch) {
        ec_futex_wait(ec_epoch_addr(ec), key.epoch);
    }
    // waiter count 감소
    atomic_fetch_add_explicit(&ec->val, EC_SUB_WAITER, memory_order_seq_cst);
}

/**
 * @brief 리눅스 커널의 Futex 대기(FUTEX_WAIT) 시스템 콜을 호출
 * 
 * @param addr 대기 상태를 비교할 32비트 메모리 주소 (Epoch의 주소)
 * @param val 현재 스레드가 기대하는(일치하는 동안 잠들어야 할) Epoch 값
 */
static inline void ec_futex_wait(uint32_t *addr, uint32_t val)
{
    syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, val, NULL, NULL, 0);
}
 
/**
 * @brief 리눅스 커널의 Futex 깨움(FUTEX_WAKE) 시스템 콜을 호출
 * 
 * @param addr 대기 중인 스레드들이 바라보고 있는 32비트 메모리 주소
 * @param n 깨울 스레드의 최대 개수 (단일 wake는 1, broadcast는 INT_MAX)
 */
static inline void ec_futex_wake(uint32_t *addr, int n)
{
    syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, n, NULL, NULL, 0);
}

/**
 * @brief 64비트 원자적 변수 내부에서 32비트 Epoch 변수가 위치한 실제 메모리 주소를 구함
 * 
 * @param ec EventCount 구조체 포인터
 * @return uint32_t* futex 시스템 콜에 직접 전달할 32비트 크기의 Epoch 주소 포인터
 */
static inline uint32_t *ec_epoch_addr(event_count_t *ec)
{
    return (uint32_t *)&ec->val + EC_EPOCH_OFFSET;
}
