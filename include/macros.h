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
 * @file    macros.h
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-06-12
 * @brief   매크로 정의 헤더 파일
 */

// ==== INCLUDES ==============================================================
// ==== DEFINES / MACROS ======================================================

#define BUF_SIZE_8K		8192
#define BUF_SIZE_4K		4096
#define BUF_SIZE_2K		2048
#define BUF_SIZE_1K		1024
#define BUF_SIZE_512    512
#define BUF_SIZE_256    256
#define BUF_SIZE_128    128
#define BUF_SIZE_64     64
#define BUF_SIZE_32     32
#define BUF_SIZE_16     16
#define BUF_SIZE_8		8

#define CACHE_LINE_SIZE 64

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define CLOSE_FD(fd) do { if ((fd) >= 0) { close(fd); (fd) = -1; } } while (0)
#define ALIGN_UP_SAFE(x, align) (((x) + (typeof(x))(align) - 1) & ~((typeof(x))(align) - 1))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

// ==== TYPEDEFS / STRUCTS ====================================================
// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================
// ==== FUNCTION PROTOTYPES ===================================================
// ==== FUNCTIONS =============================================================
