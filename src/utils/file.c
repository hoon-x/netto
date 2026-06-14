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
 * @file    file.c
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-05-10
 * @brief   파일 유틸리티 소스 파일
 */

// ==== INCLUDES ==============================================================

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include "file.h"

// ==== DEFINES / MACROS ======================================================
// ==== TYPEDEFS / STRUCTS ====================================================
// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================
// ==== FUNCTION PROTOTYPES ===================================================
// ==== FUNCTIONS =============================================================

int mkdir_p(const char *path, mode_t mode)
{
    if (path == NULL || *path == '\0') {
        return -1;
    }

    char *buf = strdup(path);
    if (buf == NULL) {
        return -1;
    }

    // trailing '/' 제거 (예: "/a/b/c/" → "/a/b/c")
    size_t len = strlen(buf);
    while (len > 0 && buf[len - 1] == '/') {
        buf[--len] = '\0';
    }

    // path가 '/'로만 이루어진 경우 (예: "///") - 이미 존재하므로 성공 반환
    if (len == 0) {
        free(buf);
        return 0;
    }

    for (char *p = buf + 1; *p != '\0'; p++) {
        if (*p == '/') {
            // 연속 슬래시 스킵: "/a//b"
            if (*(p + 1) == '/') {
                continue;
            }

            *p = '\0';

            if (mkdir(buf, mode) != 0 && errno != EEXIST) {
                free(buf);
                return -1;
            }

            *p = '/';
        }
    }

    int ret = 0;

    if (mkdir(buf, mode) != 0 && errno != EEXIST) {
        ret = -1;
    }

    free(buf);
    return ret;
}

