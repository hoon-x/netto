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
 * @file    nt_time.h
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-05-17
 * @brief   시간 관련 유틸리티 헤더 파일
 */

#ifndef _NT_TIME_H
#define _NT_TIME_H

// ==== INCLUDES ==============================================================

#include <stdint.h>

// ==== DEFINES / MACROS ======================================================
// ==== TYPEDEFS / STRUCTS ====================================================
// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================
// ==== FUNCTION PROTOTYPES ===================================================
// ==== FUNCTIONS =============================================================

/**
 * @brief 현재 시간을 나노초 단위로 반환하는 함수
 * 
 * @return uint64_t 현재 시간 (나노초)
 */
uint64_t get_now_time_ns(void);

#endif /* _NT_TIME_H */
