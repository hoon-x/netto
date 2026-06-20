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
 * @file    thread_mgr.h
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-06-20
 * @brief   쓰레드 관리 기능 구현 헤더 파일
 */

#ifndef _THREAD_MGR_H
#define _THREAD_MGR_H

// ==== INCLUDES ==============================================================

#include <pthread.h>
#include <stdbool.h>

// ==== DEFINES / MACROS ======================================================

/*
 * 컴파일타임 정적 등록 매크로.
 * 사용 예:
 *   static thread_mgr_t g_threads[] = {
 *       THREAD_ENTRY("worker", worker_thread_func, &g_worker_payload, NULL),
 *       THREAD_ENTRY("logger", logger_thread_func, NULL, NULL),
 *   };
 */
#define THREAD_ENTRY(_name, _func, _arg, _wakeup) \
    {.name = (_name), .func = (_func), .arg = (_arg), .wakeup = (_wakeup), \
    .tid = 0, .running = false}

// ==== TYPEDEFS / STRUCTS ====================================================

struct thread_mgr;
typedef void *(*thread_func_t)(struct thread_mgr *self);
typedef void (*thread_wakeup_t)(struct thread_mgr *self);

// 쓰레드 관리 구조체
typedef struct thread_mgr {
    const char *name;           // 쓰레드 식별용 이름
    thread_func_t func;         // 쓰레드 진입 함수 - thread_mgr_t *self를 받음
    void *arg;                  // 쓰레드 함수 인자값 (self->arg로 접근)
    thread_wakeup_t wakeup;     // stop 시 running=false 세팅 직후 호출됨

    pthread_t tid;              // 실행 중 채워지는 핸들
    bool running;               // 현재 가동 여부
} thread_mgr_t;

// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================
// ==== FUNCTION PROTOTYPES ===================================================

/**
 * @brief 단일 쓰레드 실행
 * 
 * @param entry 쓰레드 관리 정보 구조체
 * @return int 성공 시 0, 실패 시 -1
 */
int start_thread_mgr_one(thread_mgr_t *entry);

/**
 * @brief 단일 쓰레드 종료
 * 
 * @param entry 쓰레드 관리 정보 구조체
 */
void stop_thread_mgr_one(thread_mgr_t *entry);

/**
 * @brief 등록되어있는 모든 쓰레드 실행
 * 
 * @param threads 쓰레드 관리 정보 구조체 리스트
 * @param count 쓰레드 개수
 * @return int 성공 시 0, 실패 시 -1
 */
int start_thread_mgr_all(thread_mgr_t *threads, size_t count);

/**
 * @brief 등록되어있는 모든 쓰레드 종료
 * 
 * @param threads 쓰레드 관리 정보 구조체 리스트
 * @param count 쓰레드 개수
 */
void stop_thread_mgr_all(thread_mgr_t *threads, size_t count);

// ==== FUNCTIONS =============================================================

#endif // _THREAD_MGR_H
