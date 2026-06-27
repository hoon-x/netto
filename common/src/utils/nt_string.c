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
 * @file    nt_string.c
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-06-26
 * @brief   문자열 관련 유틸리티 기능 구현 소스 파일
 */

// ==== INCLUDES ==============================================================

#include <string.h>
#include <ctype.h>

#include "utils/nt_string.h"

// ==== DEFINES / MACROS ======================================================
// ==== TYPEDEFS / STRUCTS ====================================================
// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================
// ==== FUNCTION PROTOTYPES ===================================================
// ==== FUNCTIONS =============================================================

char *trim(char *str)
{
    if (str == NULL || *str == '\0') {
        return str;
    }

    while (isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == '\0') {
        return str;
    }

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }
    *(end + 1) = '\0';

    return str;
}

char *ltrim(char *str)
{
    if (str == NULL || *str == '\0') {
        return str;
    }

    while (isspace((unsigned char)*str)) {
        str++;
    }
    return str;
}

char *rtrim(char *str)
{
    if (str == NULL || *str == '\0') {
        return str;
    }

    char *end = str + strlen(str) - 1;
    while (end >= str && isspace((unsigned char)*end)) {
        end--;
    }
    *(end + 1) = '\0';

    return str;
}
