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
 * @file    mpmc_queue.h
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-06-07
 * @brief   락 프리 MPMC 큐 구현 헤더 파일
 */

#ifndef _MPMC_QUEUE_H
#define _MPMC_QUEUE_H

// ==== INCLUDES ==============================================================

#include <stddef.h>
#include <stdbool.h>

// ==== DEFINES / MACROS ======================================================
// ==== TYPEDEFS / STRUCTS ====================================================

typedef struct mpmc_queue mpmc_queue_t;

/**
 * @brief 예약된 queue 슬롯에 대한 정보를 저장하는 구조체
 * 
 * reserve_enqueue(dequeue)_mpmc()에 의해 채워지며, 소비자가 데이터를 직접 처리한 후
 * commit_enqueue(dequeue)_mpmc()를 호출하여 슬롯을 큐에 반환할 때 사용.
 * 
 * 본 구조체의 내용은 사용자가 직접 수정해서는 안 되며,
 * reserve_enqueue(dequeue)_mpmc()와 commit_enqueue(dequeue)_mpmc() 사이에서만 유효.
 * 
 * @warning
 * 동일한 reserved_slot_t에 대해 commit_enqueue(dequeue)_mpmc()를 두 번 이상 호출하거나,
 * reserve_enqueue(dequeue)_mpmc()를 다시 호출하기 전에 기존 슬롯이 commit되지 않은 상태에서
 * 구조체를 재사용하면 정의되지 않은 동작(undefined behavior)이 발생할 수 있음.
 */
typedef struct reserved_slot {
    /**
     * @brief 저장된 데이터의 시작 주소
     * 
     * 소비자는 이 포인터를 통해 슬롯 내부의 데이터에 직접 접근할 수 있음.
     * 데이터의 형식은 큐 생성 시 지정한 data_size에 의해 결정.
     */
    void *data;

    /**
     * @brief 내부 슬롯의 시작 주소
     *
     * 슬롯의 sequence 값을 갱신하여 큐에 반환할 때 사용.
     * 내부 구현용 필드이며 사용자가 직접 접근하거나 수정해서는 안됨.
     */
    char *slot;

    /**
     * @brief 슬롯을 획득한 시점의 dequeue sequence 값
     *
     * 슬롯을 큐에 반환할 때 다음 sequence 값을 계산하는 데 사용.
     * 내부 구현용 필드이며 사용자가 직접 접근하거나 수정해서는 안됨.
     */
    size_t pos;
} reserved_slot_t;

// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================
// ==== FUNCTION PROTOTYPES ===================================================

/**
 * @brief Lock-Free MPMC Queue 생성
 * 
 * 지정한 용량(capacity)을 갖는 bounded MPMC queue를 생성하고 초기화.
 * 
 * Queue의 용량은 반드시 2의 거듭제곱이어야 하며, 그렇지 않은 경우
 * queue 생성 실패.
 * 
 * - data_size == 0
 *   - 포인터 저장 모드
 *   - enqueue 시 전달된 포인터 값 자체를 저장
 *   - dequeue 시 포인터 값 반환
 * 
 * - data_size > 0
 *   - 데이터 복사 모드
 *   - enqueue 시 data_size 바이트를 슬롯 내부에 복사하여 저장
 *   - dequeue 시 저장된 데이터를 사용자 버퍼로 복사
 * 
 * 각 슬롯은 캐시 라인 크기 단위로 정렬되며, 내부적으로 sequence 번호를
 * 사용하여 MPMC 환경에서 lock 없이 동작함.
 * 
 * @param cap Queue의 최대 슬롯 개수 (반드시 2의 거듭제곱)
 * @param data_size 저장할 데이터 크기
 * 0이면 포인터 저장 모드로 동작하며, 0보다 크면 지정한 크기만큼 데이터를 복사하여 저장
 * @return mpmc_queue_t* 성공 시 MPMC queue 객체 포인터, 실패 시 NULL
 */
mpmc_queue_t *create_mpmc_queue(size_t cap, size_t data_size);

/**
 * @brief Lock-Free MPMC Queue 해제
 * 
 * @param q MPMC queue 구조체 포인터
 */
void destroy_mpmc_queue(mpmc_queue_t *q);

/**
 * @brief Queue에 데이터 삽입
 * 
 * 내부적으로 sequence 번호와 CAS(compare-and-swap)를 사용하여
 * 다중 producer 환경에서도 lock 없이 안전하게 동작.
 * 
 * @param q MPMC Queue 구조체 포인터
 * @param data 삽입할 데이터
 * @return true 데이터 삽입 성공
 * @return false 데이터 삽입 실패
 * 
 * @note
 * Queue가 가득차면 블로킹하지 않고 즉시 false 반환.
 * 
 * @warning
 * data_size == 0인 포인터 저장 모드에서는 포인터 값만 저장되며,
 * 포인터가 가리키는 객체의 내용은 복사되지 않음.
 * 따라서, enqueue 후 dequeue가 완료될 때까지 해당 객체의 생명 주기는
 * 호출자가 보장해야 함.
 */
bool enqueue_mpmc(mpmc_queue_t *q, void *data);

/**
 * @brief Queue에서 데이터를 꺼냄
 * 
 * Queue에서 항목 하나를 제거하고 사용자 버퍼에 반환.
 * 
 * 반환 방식은 queue 생성 시 지정한 data_size 값에 따라 결정됨.
 * 
 * - data_size == 0
 *   - 포인터 저장 모드
 *   - 슬롯에 저장되어 있던 포인터 값을 *data에 대입.
 *
 * - data_size > 0
 *   - 데이터 복사 모드
 *   - 슬롯에 저장된 data_size 바이트를 data가 가리키는 버퍼로 복사.
 * 
 * 내부적으로 sequence 번호와 CAS(compare-and-swap)를 사용하여
 * 다중 consumer 환경에서도 lock 없이 안전하게 동작.
 * 
 * @param q MPMC Queue 구조체 포인터
 * @param data 데이터를 저장할 출력 버퍼
 * 
 * - data_size == 0 인 경우
 *   포인터 변수를 가리키는 주소를 전달해야 함.
 *
 * - data_size > 0 인 경우
 *   data_size 바이트 이상을 저장할 수 있는 버퍼를 전달해야 함.
 * 
 * @return true 데이터 추출 성공
 * @return false 데이터 추출 실패
 * 
 * @warning
 * data_size == 0인 포인터 저장 모드에서는 포인터 값만 반환되며,
 * 포인터가 가리키는 객체의 소유권이나 수명 관리는 호출자가 책임져야 함.
 * 
 * @code
 * // 데이터 복사 모드
 * point_t point;
 * dequeue_mpmc(q, &point);
 *
 * // 포인터 저장 모드 (data_size == 0)
 * my_object_t *obj;
 * dequeue_mpmc(q, &obj);
 * @endcode
 */
bool dequeue_mpmc(mpmc_queue_t *q, void *data);

/**
 * @brief Enqueue의 다음 항목을 예약하고 슬롯 정보 반환
 * 
 * @param q MPMC Queue 구조체 포인터
 * @param res 예약된 슬롯 정보를 저장할 구조체
 * @return true 슬롯 예약 성공
 * @return false 슬롯 예약 실패
 */
bool reserve_enqueue_mpmc(mpmc_queue_t *q, reserved_slot_t *res);

/**
 * @brief Dequeue의 다음 항목을 예약하고 슬롯 정보 반환
 * 
 * 락 프리 MPMC(Multi-Producer Multi-Consumer) 큐에서 다음 항목을 제거 대상으로
 * 선점(reserve)하고, 해당 슬롯의 정보를 반환.
 * 
 * 예약에 성공하면 슬롯에 저장된 데이터는 res->data를 통해 직접 접근할 수 있으며,
 * 작업이 완료된 후 반드시 commit_dequeue_mpmc()를 호출하여 슬롯을 큐에 반환해야 함.
 * 
 * 내부적으로 sequence 번호와 CAS(compare-and-swap)를 사용하여
 * 다중 consumer 환경에서도 락 없이 안전하게 동작.
 * 
 * @param q MPMC Queue 구조체 포인터
 * @param res 예약된 슬롯 정보를 저장할 구조체
 * 
 * 성공 시 다음 정보가 채워짐
 * - res->data : 저장된 데이터의 시작 주소
 * - res->slot : 내부 슬롯 주소
 * - res->pos  : 예약 당시의 dequeue sequence 값
 * 
 * @return true 슬롯 예약 성공
 * @return false 슬롯 예약 실패
 * 
 * @warning
 * reserve_dequeue_mpmc()가 성공한 경우, 작업이 완료된 후 반드시
 * commit_dequeue_mpmc()를 호출해야 함.
 * 
 * @code
 * reserved_slot_t res;
 *
 * if (reserve_dequeue_mpmc(q, &res)) {
 *     my_data_t *data = (my_data_t *)res.data;
 *
 *     // 데이터 처리
 *     process(data);
 *
 *     // 반드시 슬롯 반환
 *     commit_dequeue_mpmc(q, &res);
 * }
 * @endcode
 */
bool reserve_dequeue_mpmc(mpmc_queue_t *q, reserved_slot_t *res);

/**
 * @brief Enqueue 예약된 슬롯의 처리를 완료하고 queue에 반환
 * 
 * @param q MPMC Queue 구조체 포인터
 * @param res reserve_enqueue_mpmc()를 통해 획득한 예약 슬롯 정보
 */
void commit_enqueue_mpmc(mpmc_queue_t *q, reserved_slot_t *res);

/**
 * @brief Dequeue 예약된 슬롯의 처리를 완료하고 queue에 반환
 * 
 * reserve_dequeue_mpmc()를 통해 선점한 슬롯의 처리가 완료되었음을 알리고,
 * 해당 슬롯을 다시 큐에서 재사용할 수 있도록 반환.
 * 
 * 내부적으로 sequence 번호를 갱신하여 생산자(enqueue)가 해당 슬롯을
 * 다시 사용할 수 있도록 함.
 * 
 * @param q MPMC Queue 구조체 포인터
 * @param res reserve_dequeue_mpmc()를 통해 획득한 예약 슬롯 정보
 * 
 * @note
 * 본 함수는 reserve_dequeue_mpmc()가 성공한 이후에만 호출해야 함
 * 
 * @warning
 * reserve_dequeue_mpmc()가 성공한 경우, 반드시 대응되는 commit_dequeue_mpmc()를 호출해야 함.
 * 
 * commit_dequeue_mpmc()를 호출하지 않으면 해당 슬롯은 영구적으로 반환되지 않으며,
 * 사용 가능한 슬롯 수가 점차 감소하여 결국 큐가 가득 찬 상태에 도달할 수 있음.
 * 
 * @warning
 * 동일한 reserved_slot_t에 대해 commit_dequeue_mpmc()를 두 번 이상 호출하면
 * 정의되지 않은 동작(undefined behavior)이 발생할 수 있음
 * 
 * @code
 * reserved_slot_t res;
 *
 * if (reserve_dequeue_mpmc(q, &res)) {
 *     my_data_t *data = (my_data_t *)res.data;
 *
 *     // 데이터 처리
 *     process(data);
 *
 *     // 슬롯 반환
 *     commit_dequeue_mpmc(q, &res);
 * }
 * @endcode
 * 
 * @see reserve_dequeue_mpmc()
 */
void commit_dequeue_mpmc(mpmc_queue_t *q, reserved_slot_t *res);

// ==== FUNCTIONS =============================================================

#endif /* _MPMC_QUEUE_H */
