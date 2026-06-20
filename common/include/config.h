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

// ==== DEFINES / MACROS ======================================================

#define VAR_DIR		"var"
#define LOG_DIR		"log"

#define NETTO_NAME  "netto"
#define PID_PATH    VAR_DIR "/." NETTO_NAME ".pid"
#define LOG_PATH    LOG_DIR "/" NETTO_NAME ".log"

// ==== TYPEDEFS / STRUCTS ====================================================

typedef struct config {
	bool running;
} config_t;

// ==== GLOBAL VARIABLES ======================================================

extern config_t g_config;

// ==== STATIC VARIABLES ======================================================
// ==== FUNCTION PROTOTYPES ===================================================
// ==== FUNCTIONS =============================================================

#endif // _CONFIG_H
