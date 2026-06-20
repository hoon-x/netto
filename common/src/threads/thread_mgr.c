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
 * @file    thread_mgr.c
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-06-20
 * @brief   쓰레드 관리 기능 구현 소스 파일
 */

// ==== INCLUDES ==============================================================

#include "threads/thread_mgr.h"

// ==== DEFINES / MACROS ======================================================
// ==== TYPEDEFS / STRUCTS ====================================================
// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================
// ==== FUNCTION PROTOTYPES ===================================================

static void *thread_trampoline(void *raw);

// ==== FUNCTIONS =============================================================

int start_thread_mgr_one(thread_mgr_t *entry)
{
    if (entry == NULL || entry->func == NULL) {
        return -1;
    }

    if (entry->running) {
        return -1;
    }

    entry->running = true;

    if (pthread_create(&entry->tid, NULL, thread_trampoline, entry) != 0) {
        entry->running = false;
        return -1;
    }

    return 0;
}

void stop_thread_mgr_one(thread_mgr_t *entry)
{
    if (entry == NULL || !entry->running) {
        return;
    }
    entry->running = false;
    if (entry->wakeup != NULL) {
        entry->wakeup(entry);
    }
    pthread_join(entry->tid, NULL);
}

int start_thread_mgr_all(thread_mgr_t *threads, size_t count)
{
    size_t i = 0, j = 0;

    if (threads == NULL || count == 0) {
        return -1;
    }

    for (i = 0; i < count; i++) {
        if (start_thread_mgr_one(&threads[i]) != 0) {
            for (j = 0; j < i; j++) {
                stop_thread_mgr_one(&threads[j]);
            }
            return -1;
        }
    }

    return 0;
}

void stop_thread_mgr_all(thread_mgr_t *threads, size_t count)
{
    int i = 0;

    if (threads == NULL || count == 0) {
        return;
    }

    for (i = count - 1; i >= 0; i--) {
        stop_thread_mgr_one(&threads[i]);
    }
}

/**
 * @brief 자기 자신의 thread_mgr_t* 인자를 받는 함수를 실행하는 쓰레드
 * 
 * @param raw 자기 자신의 thread_mgr_t* 인자
 * @return void* NULL
 */
static void *thread_trampoline(void *raw)
{
    thread_mgr_t *entry = (thread_mgr_t *)raw;
    return entry->func(entry);
}
