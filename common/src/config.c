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
 * @file    config.c
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-06-20
 * @brief   설정 관련 기능 구현 소스 파일
 */

// ==== INCLUDES ==============================================================

#include <stdatomic.h>
#include <stdint.h>
#include <stdalign.h>
#include <stddef.h>
#include <pthread.h>

#include "config.h"
#include "macros.h"
#include "utils/event_count.h"

// ==== DEFINES / MACROS ======================================================
// ==== TYPEDEFS / STRUCTS ====================================================

typedef struct config_dbuf {
    config_t conf[2];
    pthread_mutex_t writer_lock;
    _Atomic int active_idx;
    alignas(CACHE_LINE_SIZE) _Atomic uint32_t refcnt[2];
    event_count_t ec;
} config_dbuf_t;

// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================

static config_dbuf_t g_conf_dbuf;

// ==== FUNCTION PROTOTYPES ===================================================

static int load_config(config_t *cfg);

// ==== FUNCTIONS =============================================================

void runtime_config_init(runtime_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }

    atomic_init(&cfg->running, false);
}

void config_init(void)
{
    pthread_mutex_init(&g_conf_dbuf.writer_lock, NULL);
    atomic_init(&g_conf_dbuf.active_idx, 0);
    atomic_init(&g_conf_dbuf.refcnt[0], 0);
    atomic_init(&g_conf_dbuf.refcnt[1], 0);
    ec_init(&g_conf_dbuf.ec);
}

const config_t *config_acquire(void)
{
    while (true) {
        // 현재 활성화 상태의 이중버퍼 인덱스를 가져옴
        int idx = atomic_load_explicit(&g_conf_dbuf.active_idx, memory_order_relaxed);

        // 참조 카운트 증가
        atomic_fetch_add_explicit(&g_conf_dbuf.refcnt[idx], 1, memory_order_acquire);

        // 현재 활성 이중 버퍼 인덱스가 새로 가져온 인덱스와 일치한다면 설정 정보 반환
        if (atomic_load_explicit(&g_conf_dbuf.active_idx, memory_order_acquire) == idx) {
            return &g_conf_dbuf.conf[idx];
        }

        // 중간에 writer가 이중 버퍼 인덱스를 변경했다면 참조 카운트를 감소시키고 재시도
        atomic_fetch_sub_explicit(&g_conf_dbuf.refcnt[idx], 1, memory_order_release);
    }
}

void config_release(const config_t *cfg)
{
    // 현재 설정 정보 구조체의 이중 버퍼 인덱스를 구함
    int idx = (int)(cfg - g_conf_dbuf.conf);

    // 참조 카운트를 감소시키고 내가 마지막 reader인 경우 혹시 잠들어 있을 writer를 깨워줌
    uint32_t prev = atomic_fetch_sub_explicit(&g_conf_dbuf.refcnt[idx], 1, memory_order_release);
    if (prev == 1) {
        ec_notify(&g_conf_dbuf.ec);
    }
}

void config_update(const config_t *new_cfg)
{
    pthread_mutex_lock(&g_conf_dbuf.writer_lock);

    // 현재 비활성 상태의 이중 버퍼 인덱스를 구함
    int old_idx = atomic_load_explicit(&g_conf_dbuf.active_idx, memory_order_relaxed);
    int new_idx = old_idx ^ 1;

    // 설정 파일 로드
    g_conf_dbuf.conf[new_idx] = *new_cfg;

    // 이중 버퍼 인덱스 swap
    atomic_store_explicit(&g_conf_dbuf.active_idx, new_idx, memory_order_release);

    // 이전 설정을 사용하던 모든 reader들이 작업을 끝마치길 기다림
    while (true) {
        ec_key_t key = ec_prepare_wait(&g_conf_dbuf.ec);

        if (atomic_load_explicit(&g_conf_dbuf.refcnt[old_idx], memory_order_acquire) == 0) {
            ec_cancel_wait(&g_conf_dbuf.ec);
            break;
        }

        ec_wait(&g_conf_dbuf.ec, key);
    }

    pthread_mutex_unlock(&g_conf_dbuf.writer_lock);
}

static int load_config(config_t *cfg)
{
    return 0;
}
