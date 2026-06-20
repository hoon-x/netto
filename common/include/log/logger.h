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
// ==== DEFINES / MACROS ======================================================

#define LOG_DEBUG(fmt, ...) write_daemon_log(LOG_LVL_DEBUG, "[%s():%d] ", __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) write_daemon_log(LOG_LVL_INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) write_daemon_log(LOG_LVL_WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) write_daemon_log(LOG_LVL_ERROR, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) write_daemon_log(LOG_LVL_FATAL, fmt, ##__VA_ARGS__)

// ==== TYPEDEFS / STRUCTS ====================================================

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

/**
 * @brief   데몬 로거 초기화
 * @param   log_path        로그 파일 경로
 * @param   max_log_size    최대 로그 사이즈 (단위: MB)
 * @param   max_backup      로그 파일 백업 개수
 * @param   debug           디버그 모드 플래그
 * @return  int             성공 시 0, 실패 시 -1
 */
int init_daemon_logger(const char *log_path, int max_log_size, int max_backup, bool debug);

/**
 * @brief 데몬 로거 메모리 해제
 * 
 */
void destroy_daemon_logger(void);

// ==== FUNCTIONS =============================================================

#endif /* _LOGGER_H */
