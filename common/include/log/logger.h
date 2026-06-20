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
 * @file    logger.h
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-06-02
 * @brief   로깅 유틸리티 헤더 파일
 */

#ifndef _LOGGER_H
#define _LOGGER_H

// ==== INCLUDES ==============================================================

#include "threads/thread_mgr.h"

// ==== DEFINES / MACROS ======================================================

#define LOG_DEBUG(fmt, ...) enqueue_daemon_log(LEVEL_DEBUG, "[%s():%d] ", __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) enqueue_daemon_log(LEVEL_INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) enqueue_daemon_log(LEVEL_WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) enqueue_daemon_log(LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) enqueue_daemon_log(LEVEL_FATAL, fmt, ##__VA_ARGS__)

// ==== TYPEDEFS / STRUCTS ====================================================

// 로그 레벨 정의
typedef enum log_level {
    LEVEL_DEBUG = 0,
    LEVEL_INFO,
    LEVEL_WARN,
    LEVEL_ERROR,
    LEVEL_FATAL,
    LEVEL_MAX
} log_level_t;

// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================
// ==== FUNCTION PROTOTYPES ===================================================

/**
 * @brief   데몬 로거 초기화
 * @param   log_path        로그 파일 경로
 * @param   max_log_size    최대 로그 사이즈 (단위: MB)
 * @param   max_backup      로그 파일 백업 개수 (0이면 백업 안함)
 * @param   debug           디버그 모드 플래그
 * @return  int             성공 시 0, 실패 시 -1
 */
int init_daemon_logger(const char *log_path, int max_log_size, int max_backup, bool debug);

/**
 * @brief 데몬 로거 메모리 해제
 * 
 */
void destroy_daemon_logger(void);

/**
 * @brief 데몬 로그 수집 쓰레드
 * 
 * @param self 자신의 쓰레드 관리 정보 구조체
 * @return void* NULL
 */
void *daemon_log_consumer_thread(thread_mgr_t *self);

/**
 * @brief 데몬 로그 수집 쓰레드 깨우는 함수
 * 
 * 종료 신호를 받았을 때, 쓰레드가 sleep 상태면 깨워줌
 * 
 * @param self 자신의 쓰레드 관리 정보 구조체
 */
void wake_daemon_log_consumer_thread(thread_mgr_t *self);

/**
 * @brief MPMC 큐에 로그를 삽입하는 함수
 * 
 * @param level 로그 레벨
 * @param fmt 로그 메시지
 */
void enqueue_daemon_log(log_level_t level, const char *fmt, ...);

// ==== FUNCTIONS =============================================================

#endif /* _LOGGER_H */
