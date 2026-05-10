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
 * @file    process.h
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-05-05
 * @brief   프로세스 유틸리티 헤더 파일
 */

#ifndef _PROCESS_H
#define _PROCESS_H

// ==== INCLUDES ==============================================================

#include <sys/types.h>
#include <stdbool.h>

// ==== DEFINES / MACROS ======================================================
// ==== TYPEDEFS / STRUCTS ====================================================
// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================
// ==== FUNCTION PROTOTYPES ===================================================

/**
 * @brief 프로세스가 실행 중인지 확인하는 함수
 * 
 * @param pid 확인하려는 프로세스의 ID
 * @return bool - true(실행 중), false(종료됨 또는 존재하지 않음)
 */
bool is_process_running(pid_t pid);

/**
 * @brief /proc/<pid>/comm에서 프로세스 명을 읽어오는 함수
 * 
 * 15자까지 읽어오며, 개행 문자는 제거됨
 * 
 * @param pid 확인하려는 프로세스의 ID
 * @param out 프로세스 명이 저장될 버퍼
 * @param out_size 버퍼 크기
 * @return int - 성공 시 0, 실패 시 -1
 */
int get_procname_comm(pid_t pid, char *out, size_t out_size);

/**
 * @brief /proc/<pid>/cmdline에서 프로세스 명을 읽어오는 함수
 * 
 * @param pid 확인하려는 프로세스의 ID
 * @param out 프로세스 명이 저장될 버퍼
 * @param out_size 버퍼 크기
 * @param basename_only basename만 반환할지 여부
 * @return int - 성공 시 0, 실패 시 -1
 */
int get_procname_cmdline(pid_t pid, char *out, size_t out_size, bool basename_only);

/**
 * @brief 프로세스를 데몬화하는 함수
 * 
 * @return int - 성공 시 0, 실패 시 -1
 */
int daemonize(void);

/**
 * @brief 표준 입력/출력/오류를 /dev/null로 리디렉션하는 함수
 * 
 * @return int - 성공 시 0, 실패 시 -1
 */
int redirect_devnull(void);

// ==== FUNCTIONS =============================================================

#endif // _PROCESS_H
