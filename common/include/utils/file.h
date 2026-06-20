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
 * @file    file.h
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-05-10
 * @brief   파일 유틸리티 헤더 파일
 */

#ifndef _FILE_H
#define _FILE_H

// ==== INCLUDES ==============================================================

#include <sys/types.h>

// ==== DEFINES / MACROS ======================================================
// ==== TYPEDEFS / STRUCTS ====================================================
// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================
// ==== FUNCTION PROTOTYPES ===================================================

/**
 * @brief   디렉토리를 재귀적으로 생성하는 함수
 * @param   path    생성할 디렉토리 경로
 * @param   mode    디렉토리 생성 시 사용할 권한
 * @return  성공 시 0, 실패 시 -1
 */
int mkdir_p(const char *path, mode_t mode);

// ==== FUNCTIONS =============================================================

#endif /* _FILE_H */
