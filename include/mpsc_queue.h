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
 * @file    mpsc_queue.h
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-06-09
 * @brief   락 프리 MPSC 큐 구현 헤더 파일
 */

#ifndef _MPSC_QUEUE_H
#define _MPSC_QUEUE_H

// ==== INCLUDES ==============================================================

#include <stddef.h>
#include <stdbool.h>

// ==== DEFINES / MACROS ======================================================
// ==== TYPEDEFS / STRUCTS ====================================================

typedef struct mpsc_queue mpsc_queue_t;

// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================
// ==== FUNCTION PROTOTYPES ===================================================

/**
 * @brief 락 프리 MPSC 큐 초기화 함수
 * 
 * @param cap 큐의 용량 (항상 2의 거듭제곱이어야 함)
 * @return 초기화된 MPSC 큐 포인터, 실패 시 NULL
 */
mpsc_queue_t *init_mpsc_queue(size_t cap);

/**
 * @brief 락 프리 MPSC 큐 메모리 해제 함수
 * 
 * @param q 해제할 MPSC 큐 포인터
 */
void free_mpsc_queue(mpsc_queue_t *q);

/**
 * @brief 락 프리 MPSC 큐에 데이터를 추가하는 함수
 * 
 * @param q 데이터를 추가할 MPSC 큐 포인터
 * @param data 추가할 데이터 포인터
 * @return true(성공), false(큐가 가득 찼거나 오류 발생)
 */
bool enqueue_mpsc(mpsc_queue_t *q, void *data);

/**
 * @brief 락 프리 MPSC 큐에서 데이터를 가져오는 함수
 * 
 * @param q 데이터를 추가할 MPSC 큐 포인터
 * @param data 가져온 데이터가 저장될 포인터
 * @return true(성공), false(가져올 데이터가 없음)
 */
bool dequeue_mpsc(mpsc_queue_t *q, void **data);

// ==== FUNCTIONS =============================================================

#endif /* _MPSC_QUEUE_H */
