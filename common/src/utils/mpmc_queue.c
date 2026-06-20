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
 * @file    mpmc_queue.c
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-06-07
 * @brief   락 프리 MPMC 큐 구현 소스 파일
 */

// ==== INCLUDES ==============================================================

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdalign.h>

#include "utils/mpmc_queue.h"
#include "macros.h"

// ==== DEFINES / MACROS ======================================================
// ==== TYPEDEFS / STRUCTS ====================================================

struct mpmc_queue {
    char *slots;
    size_t slot_mask;
    size_t slot_stride;
    size_t data_offset;
    size_t data_size;

    alignas(CACHE_LINE_SIZE) atomic_size_t enqueue_pos;
    alignas(CACHE_LINE_SIZE) atomic_size_t dequeue_pos;
};

// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================
// ==== FUNCTION PROTOTYPES ===================================================

static bool reserve_enqueue_slot_internal(mpmc_queue_t *q, reserved_slot_t *res);
static bool reserve_dequeue_slot_internal(mpmc_queue_t *q, reserved_slot_t *res);

// ==== FUNCTIONS =============================================================

mpmc_queue_t *create_mpmc_queue(size_t cap, size_t data_size)
{
    // Capacity는 반드시 2의 거듭제곱이어야 함
    if (cap == 0 || (cap & (cap - 1)) != 0) {
        return NULL;
    }

    mpmc_queue_t *q = (mpmc_queue_t *)aligned_alloc(CACHE_LINE_SIZE, sizeof(mpmc_queue_t));
    if (q == NULL) {
        return NULL;
    }
    memset(q, 0, sizeof(mpmc_queue_t));

    // 모드 구분을 위해 원래 값 저장
    q->data_size = data_size;
    // data_size가 0이면 포인터 저장 모드, 아니면 실제 데이터 크기 사용
    data_size = (data_size == 0) ? sizeof(void *) : data_size;
    // 각 슬롯 별 데이터 시작 위치
    q->data_offset = ALIGN_UP_SAFE(sizeof(atomic_size_t), alignof(max_align_t));
    // 각 슬롯의 stride 계산
    q->slot_stride = ALIGN_UP_SAFE(q->data_offset + data_size, CACHE_LINE_SIZE);
    q->slot_mask = cap - 1;

    q->slots = (char *)aligned_alloc(CACHE_LINE_SIZE, cap * q->slot_stride);
    if (q->slots == NULL) {
        free(q);
        return NULL;
    }
    memset(q->slots, 0, cap * q->slot_stride);

    // 모든 슬롯의 seq 값을 해당 슬롯의 인덱스로 초기화
    for (size_t i = 0; i < cap; i++) {
        char *slot = q->slots + (i * q->slot_stride);
        atomic_init((atomic_size_t *)slot, i);
    }

    atomic_init(&q->enqueue_pos, 0);
    atomic_init(&q->dequeue_pos, 0);

    return q;
}

void destroy_mpmc_queue(mpmc_queue_t *q)
{
    if (q != NULL) {
        free(q->slots);
        free(q);
    }
}

bool enqueue_mpmc(mpmc_queue_t *q, void *data)
{
    reserved_slot_t res;

    if (q == NULL || data == NULL) {
        return false;
    }

    if (!reserve_enqueue_slot_internal(q, &res)) {
        return false;
    }

    // 데이터 삽입 방식 분기:
    // 1. data_size == 0 이면 포인터 직접 저장 (대입 방식)
    // 2. data_size > 0 이면 메모리 블록 복사 (memcpy 방식)
    if (q->data_size == 0) {
        *(void **)res.data = data;
    }
    else {
        memcpy(res.data, data, q->data_size);
    }

    commit_enqueue_mpmc(q, &res);
    return true;
}

bool dequeue_mpmc(mpmc_queue_t *q, void *data)
{
    reserved_slot_t res;

    if (q == NULL || data == NULL) {
        return false;
    }

    if (!reserve_dequeue_slot_internal(q, &res)) {
        return false;
    }

    if (q->data_size == 0) {
        *(void **)data = *(void **)res.data;
    }
    else {
        memcpy(data, res.data, q->data_size);
    }

    commit_dequeue_mpmc(q, &res);
    return true;
}

bool reserve_enqueue_mpmc(mpmc_queue_t *q, reserved_slot_t *res)
{
    if (q == NULL || res == NULL) {
        return false;
    }
    return reserve_enqueue_slot_internal(q, res);
}

bool reserve_dequeue_mpmc(mpmc_queue_t *q, reserved_slot_t *res)
{
    if (q == NULL || res == NULL) {
        return false;
    }
    return reserve_dequeue_slot_internal(q, res);
}

void commit_enqueue_mpmc(mpmc_queue_t *q, reserved_slot_t *res)
{
    if (q == NULL || res == NULL) {
        return;
    }
    atomic_store_explicit((atomic_size_t *)res->slot, res->pos + 1, memory_order_release);
}

void commit_dequeue_mpmc(mpmc_queue_t *q, reserved_slot_t *res)
{
    if (q == NULL || res == NULL) {
        return;
    }
    atomic_store_explicit((atomic_size_t *)res->slot, res->pos + q->slot_mask + 1, memory_order_release);
}

/**
 * @brief Enqueue의 다음 슬롯을 내부적으로 선점
 * 
 * @param q MPMC Queue 구조체 포인터
 * @param res 예약된 슬롯 정보를 저장할 구조체
 * @return true 슬롯 선점 성공
 * @return false 큐가 가득차서 선점할 수 있는 슬롯이 없음
 */
static bool reserve_enqueue_slot_internal(mpmc_queue_t *q, reserved_slot_t *res)
{
    size_t pos = atomic_load_explicit(&q->enqueue_pos, memory_order_relaxed);

    while (true) {
        char *slot = q->slots + ((pos & q->slot_mask) * q->slot_stride);
        atomic_size_t *seq = (atomic_size_t *)slot;

        size_t s = atomic_load_explicit(seq, memory_order_acquire);
        intptr_t diff = (intptr_t)s - (intptr_t)pos;

        if (diff == 0) {
            if (atomic_compare_exchange_weak_explicit(&q->enqueue_pos, &pos, pos + 1,
                memory_order_relaxed, memory_order_relaxed))
            {
                res->data = slot + q->data_offset;
                res->slot = slot;
                res->pos = pos;
                return true;
            }
        }
        else if (diff < 0) {
            return false;
        }
        else {
            // 다른 쓰레드가 먼저 선점함, 최신 pos로 갱신 후 재시도
            pos = atomic_load_explicit(&q->enqueue_pos, memory_order_relaxed);
        }
    }
}

/**
 * @brief Dequeue의 다음 슬롯을 내부적으로 선점
 * 
 * dequeue_pos를 원자적으로 증가시켜 다음 소비 대상 슬롯을 확보하고,
 * 해당 슬롯에 대한 예약 정보를 res에 기록.
 *
 * 슬롯 확보에 성공하면 다음 정보 설정.
 * - res->data : 저장된 데이터의 시작 주소
 * - res->slot : 내부 슬롯 주소
 * - res->pos  : 슬롯을 획득한 시점의 dequeue sequence 값
 * 
 * @param q MPMC Queue 구조체 포인터
 * @param res 예약된 슬롯 정보를 저장할 구조체
 * @return true 슬롯 선점 성공
 * @return false 큐가 비어 있어 선점할 수 있는 슬롯이 없음
 * 
 * @note
 * 슬롯을 성공적으로 획득한 경우, 해당 슬롯은 이후
 * commit_dequeue_mpmc() 또는 dequeue_mpmc() 내부 처리에 의해
 * 큐로 반환되어야 함.
 */
static bool reserve_dequeue_slot_internal(mpmc_queue_t *q, reserved_slot_t *res)
{
    size_t pos = atomic_load_explicit(&q->dequeue_pos, memory_order_relaxed);

    while (true) {
        char *slot = q->slots + ((pos & q->slot_mask) * q->slot_stride);
        atomic_size_t *seq = (atomic_size_t *)slot;

        size_t s = atomic_load_explicit(seq, memory_order_acquire);
        intptr_t diff = (intptr_t)s - (intptr_t)(pos + 1);

        if (diff == 0) {
            if (atomic_compare_exchange_weak_explicit(&q->dequeue_pos, &pos, pos + 1,
                memory_order_relaxed, memory_order_relaxed))
            {
                res->data = slot + q->data_offset;
                res->slot = slot;
                res->pos = pos;
                return true;
            }
        }
        else if (diff < 0) {
            return false;
        }
        else {
            pos = atomic_load_explicit(&q->dequeue_pos, memory_order_relaxed);
        }
    }
}
