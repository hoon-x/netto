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
 * @file    version.h
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-05-05
 * @brief   빌드 및 버전 정보 헤더 파일
 */

#ifndef _VERSION_H
#define _VERSION_H

// ==== INCLUDES ==============================================================
// ==== DEFINES / MACROS ======================================================

// 빌드 시 정보 주입됨
#ifndef VERSION
#define VERSION         "unknown"
#endif

#ifndef GIT_HASH
#define GIT_HASH        "unknown"
#endif

#ifndef BUILD_DATE
#define BUILD_DATE      "unknown"
#endif

#ifndef BUILD_OS
#define BUILD_OS        "unknown"
#endif

#ifndef BUILD_KERN_VER
#define BUILD_KERN_VER  "unknown"
#endif

#ifndef BUILD_ARCH
#define BUILD_ARCH      "unknown"
#endif

// ==== TYPEDEFS / STRUCTS ====================================================
// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================
// ==== FUNCTION PROTOTYPES ===================================================
// ==== FUNCTIONS =============================================================

#endif // _VERSION_H
