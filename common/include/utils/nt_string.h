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
 * @file    nt_string.h
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-06-26
 * @brief   문자열 관련 유틸리티 기능 구현 헤더 파일
 */

#ifndef _NT_STRING_H
#define _NT_STRING_H

// ==== INCLUDES ==============================================================
// ==== DEFINES / MACROS ======================================================
// ==== TYPEDEFS / STRUCTS ====================================================
// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================
// ==== FUNCTION PROTOTYPES ===================================================

/**
 * @brief 문자열 좌/우 whitespace 제거
 * 
 * @param str 문자열
 * @return char* 수정된 문자열의 시작 포인터
 */
char *trim(char *str);

/**
 * @brief 문자열 좌측 whitespace 제거
 * 
 * @param str 문자열
 * @return char* 수정된 문자열의 시작 포인터
 */
char *ltrim(char *str);

/**
 * @brief 문자열 우측 whitespace 제거
 * 
 * @param str 문자열
 * @return char* 수정된 문자열의 시작 포인터
 */
char *rtrim(char *str);

// ==== FUNCTIONS =============================================================

#endif  // _NT_STRING_H
