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
 * @file    config.h
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-05-04
 * @brief   전역 설정 정보 헤더 파일
 */

#ifndef _CONFIG_H
#define _CONFIG_H

// ==== INCLUDES ==============================================================

#include <stdbool.h>
#include <stdatomic.h>

// ==== DEFINES / MACROS ======================================================

#define VAR_DIR		"var"
#define LOG_DIR		"log"
#define CONF_DIR	"conf"

#define NETTO_NAME  "netto"
#define PID_PATH    VAR_DIR "/." NETTO_NAME ".pid"
#define LOG_PATH    LOG_DIR "/" NETTO_NAME ".log"
#define CONF_PATH	CONF_DIR "/" NETTO_NAME ".ini"

// ==== TYPEDEFS / STRUCTS ====================================================

typedef struct config {
	int max_log_size;
	int max_backup;
} config_t;

typedef struct runtime_config {
	_Atomic bool running;
	bool debug;
} runtime_config_t;

typedef int (*cfg_parser_fn)(config_t *cfg, const char *section, const char *key, const char *value);

// ==== GLOBAL VARIABLES ======================================================

extern runtime_config_t g_runconf;

// ==== STATIC VARIABLES ======================================================
// ==== FUNCTION PROTOTYPES ===================================================

/**
 * @brief 런타임 설정 구조체 초기화
 * 
 * @param cfg 런타임 설정 정보 구조체 포인터
 */
void runtime_config_init(runtime_config_t *cfg);

/**
 * @brief 설정 정보 관리 구조체 초기화
 */
void config_init(void);

/**
 * @brief 설정 정보 관리 구조체에서 현재 유효한 설정 정보를 가져옴
 * 
 * @return const config_t* 설정 정보 구조체
 */
const config_t *config_acquire(void);

/**
 * @brief acquire로 가져온 설정 정보 사용을 끝마쳤음을 나타냄
 * 
 * @param cfg acquire로 가져온 설정 정보 구조체 포인터
 */
void config_release(const config_t *cfg);

/**
 * @brief 설정 정보 업데이트
 * 
 * @param conf_path 설정 파일 경로
 * @param parser_fn 파싱 콜백 함수
 * @return int 성공 시 0, 실패 시 -1
 */
int config_update(const char *conf_path, cfg_parser_fn parser_fn);

/**
 * @brief netto 설정 파싱 콜백 함수
 * 
 * @param cfg 설정 정보 구조체
 * @param section 섹션
 * @param key 키
 * @param value 값
 * @return int 성공 시 0, 실패 시 -1
 */
int netto_config_parser(config_t *cfg, const char *section, const char *key, const char *value);

// ==== FUNCTIONS =============================================================

#endif // _CONFIG_H
