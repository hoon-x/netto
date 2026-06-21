// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
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
 *
 * --------------------------------------------------------------------
 * MODIFICATION NOTICE:
 * This file is a C11 port of folly::EventCount, originally implemented
 * in C++ as part of the Folly library (https://github.com/facebook/folly,
 * folly/synchronization/EventCount.h).
 * Ported and modified by JongHoon Shim (shim9532@gmail.com), June 2026.
 */

// ==== DESCRIPTION ===========================================================

/**
 * @file    event_count.h
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-06-09
 * @brief   Lock-Free 알고리즘용 조건 변수 구현 헤더 파일
 */

#ifndef _EVENT_COUNT_H
#define _EVENT_COUNT_H

/*
 * 레이아웃:
 *
 *  val_ (uint64):
 *   ┌──────────────────────┬──────────────────────┐
 *   │  epoch  [63:32]      │  waiter_count [31:0] │
 *   └──────────────────────┴──────────────────────┘
 *
 * futexWait/Wake 대상: epoch 워드 (상위 32비트)
 *   - Little-endian: &val + 4 (offset 1 * sizeof(uint32_t))
 *   - Big-endian:    &val + 0 (offset 0 * sizeof(uint32_t))
*/

// ==== INCLUDES ==============================================================

#include <stdatomic.h>
#include <stdint.h>
#include <stdalign.h>

#include "macros.h"

// ==== DEFINES / MACROS ======================================================
// ==== TYPEDEFS / STRUCTS ====================================================

typedef struct __attribute__((aligned(CACHE_LINE_SIZE))) event_count {
    _Atomic uint64_t val;
} event_count_t;

// Key: prepare_wait() 시점의 epoch 스냅샷
typedef struct ec_key {
    uint32_t epoch;
} ec_key_t;

// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================
// ==== FUNCTION PROTOTYPES ===================================================

/**
 * @brief EventCount 초기화
 * 
 * @param ec EventCount 구조체 포인터
 */
void ec_init(event_count_t *ec);

/**
 * @brief 대기 중인 스레드 중 하나를 깨움
 * 
 * @param ec EventCount 구조체 포인터
 */
void ec_notify(event_count_t *ec);

/**
 * @brief 대기 중인 모든 스레드를 깨움
 * 
 * @param ec EventCount 구조체 포인터
 */
void ec_notify_all(event_count_t *ec);

/**
 * @brief 현재 epoch를 key 돌려받음 (슬립 전 호출)
 * 
 * waiter count를 원자적으로 +1하고, 그 시점의 epoch를 스냅샷함.
 * 반환된 key를 ec_wait() 또는 ec_cancel_wait()에 전달해야 함.
 * 
 * @param ec EventCount 구조체 포인터
 * @return ec_key_t 실제 대기 처리 함수(ec_wait)에서 변경 여부를 검증할 현재 Epoch 키
 */
ec_key_t ec_prepare_wait(event_count_t *ec);

/**
 * @brief 조건이 이미 충족된 경우 waiter count를 되돌림
 * 
 * memory_order_seq_cst: waiter_count가 빨리 0에 도달할수록
 * 불필요한 futex_wake syscall을 줄일 수 있음.
 * 
 * @param ec EventCount 구조체 포인터
 */
void ec_cancel_wait(event_count_t *ec);

/**
 * @brief epoch가 바뀔 때까지 슬립
 * 
 * epoch가 여전히 key.epoch와 같으면 futex_wait으로 블록.
 * epoch가 달라지면(= notify가 왔으면) 반환하고 waiter count를 -1.
 * 
 * spurious wakeup 허용: 호출자가 루프에서 재검사해야 한다.
 * 
 * @param ec EventCount 구조체 포인터
 * @param key ec_prepare_wait 함수로부터 전달받은 검증용 키
 */
void ec_wait(event_count_t *ec, ec_key_t key);

// ==== FUNCTIONS =============================================================

#endif // _EVENT_COUNT_H
