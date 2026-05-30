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
 * @file    shm_logger.h
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-05-15
 * @brief   공유 메모리 로깅 유틸리티 헤더 파일
 */

// ==== INCLUDES ==============================================================
// ==== DEFINES / MACROS ======================================================

#define LOG_DEBUG(fmt, ...) write_daemon_log(LOG_LVL_DEBUG, "[%s():%d] ", __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) write_daemon_log(LOG_LVL_INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) write_daemon_log(LOG_LVL_WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) write_daemon_log(LOG_LVL_ERROR, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) write_daemon_log(LOG_LVL_FATAL, fmt, ##__VA_ARGS__)

// ==== TYPEDEFS / STRUCTS ====================================================

// 로그 타입 정의
typedef enum log_type {
    LOG_TYPE_DAEMON = 0,
    LOG_TYPE_MAX
} log_type_t;

// 로그 레벨 정의
typedef enum log_level {
    LOG_LVL_DEBUG = 0,
    LOG_LVL_INFO,
    LOG_LVL_WARN,
    LOG_LVL_ERROR,
    LOG_LVL_FATAL,
    LOG_LVL_MAX
} log_level_t;

// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================
// ==== FUNCTION PROTOTYPES ===================================================
// ==== FUNCTIONS =============================================================

/**
 * @brief Daemon 로거 consumer 초기화
 * @return int 초기화 성공 시 0, 실패 시 -1
 */
int init_daemon_logger_consumer(void);

/**
 * @brief Daemon 로거 producer 초기화
 * @return int 초기화 성공 시 0, 실패 시 -1
 */
int init_daemon_logger_producer(void);

/**
 * @brief 로거 리소스 해제
 * @param type 로그 타입
 * @param is_consumer consumer인지 여부
 */
void destroy_logger(log_type_t type, bool is_consumer);

/**
 * @brief Daemon 로그 작성
 * @param level 로그 레벨
 * @param fmt 포맷 문자열
 * @param ... 가변 인자
 */
void write_daemon_log(log_level_t level, const char *fmt, ...);

/**
 * @brief Daemon 로그 consumer 스레드 함수
 * @param arg 스레드 인자
 * @return void * 스레드 반환 값
 */
void *daemon_log_consumer_thread(void *arg);
