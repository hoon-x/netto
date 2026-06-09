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

#include "mpmc_queue.h"

// ==== DEFINES / MACROS ======================================================

#define CACHE_LINE_SIZE 64

// ==== TYPEDEFS / STRUCTS ====================================================

typedef struct cell {
    atomic_size_t seq;
    void *data;
} __attribute__((aligned(CACHE_LINE_SIZE))) cell_t;

struct mpmc_queue {
    cell_t *buffer;
    size_t mask;

    alignas(CACHE_LINE_SIZE) atomic_size_t enqueue_pos;
    alignas(CACHE_LINE_SIZE) atomic_size_t dequeue_pos;
};

// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================
// ==== FUNCTION PROTOTYPES ===================================================
// ==== FUNCTIONS =============================================================

mpmc_queue_t *init_mpmc_queue(size_t cap)
{
    if (cap == 0 || (cap & (cap - 1)) != 0) {
        return NULL;
    }

    mpmc_queue_t *q = (mpmc_queue_t *)aligned_alloc(CACHE_LINE_SIZE, sizeof(mpmc_queue_t));
    if (q == NULL) {
        return NULL;
    }
    memset(q, 0, sizeof(mpmc_queue_t));

    q->buffer = (cell_t *)aligned_alloc(CACHE_LINE_SIZE, cap * sizeof(cell_t));
    if (q->buffer == NULL) {
        free(q);
        return NULL;
    }
    memset(q->buffer, 0, cap * sizeof(cell_t));

    q->mask = cap - 1;

    for (size_t i = 0; i < cap; i++) {
        atomic_init(&q->buffer[i].seq, i);
    }

    atomic_init(&q->enqueue_pos, 0);
    atomic_init(&q->dequeue_pos, 0);
    
    return q;
}

void free_mpmc_queue(mpmc_queue_t *q)
{
    if (q != NULL) {
        if (q->buffer != NULL) {
            free(q->buffer);
        }
        free(q);
    }
}

bool enqueue_mpmc(mpmc_queue_t *q, void *data)
{
    cell_t *cell = NULL;

    if (q == NULL || data == NULL) {
        return false;
    }

    size_t pos = atomic_load_explicit(&q->enqueue_pos, memory_order_relaxed);

    while (true) {
        cell = &q->buffer[pos & q->mask];
        size_t seq = atomic_load_explicit(&cell->seq, memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)pos;

        if (diff == 0) {
            if (atomic_compare_exchange_weak_explicit(&q->enqueue_pos, &pos, pos + 1,
                memory_order_relaxed, memory_order_relaxed))
            {
                break;
            }
        }
        else if (diff < 0) {
            return false;
        }
        else {
            pos = atomic_load_explicit(&q->enqueue_pos, memory_order_relaxed);
        }
    }
    
    cell->data = data;
    atomic_store_explicit(&cell->seq, pos + 1, memory_order_release);
    return true;
}

bool dequeue_mpmc(mpmc_queue_t *q, void **data)
{
    cell_t *cell = NULL;

    if (q == NULL || data == NULL) {
        return false;
    }

    size_t pos = atomic_load_explicit(&q->dequeue_pos, memory_order_relaxed);

    while (true) {
        cell = &q->buffer[pos & q->mask];
        size_t seq = atomic_load_explicit(&cell->seq, memory_order_acquire);
        intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);

        if (diff == 0) {
            if (atomic_compare_exchange_weak_explicit(&q->dequeue_pos, &pos, pos + 1,
                memory_order_relaxed, memory_order_relaxed))
            {
                break;
            }
        }
        else if (diff < 0) {
            return false;
        }
        else {
            pos = atomic_load_explicit(&q->dequeue_pos, memory_order_relaxed);
        }
    }

    *data = cell->data;
    atomic_store_explicit(&cell->seq, pos + q->mask + 1, memory_order_release);
    return true;
}
