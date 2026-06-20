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
 * @file    logger.c
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-06-02
 * @brief   로깅 유틸리티 소스 파일
 */

// ==== INCLUDES ==============================================================

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <libgen.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "log/logger.h"
#include "utils/nt_time.h"
#include "utils/file.h"
#include "macros.h"

// ==== DEFINES / MACROS ======================================================

#define DEF_MAX_LOG_SIZE    (10 * 1024 * 1024)	// 10MB

// ==== TYPEDEFS / STRUCTS ====================================================

// Daemon 로그 슬롯 구조체
typedef struct daemon_log_slot {
    uint64_t timestamp_ns;
    uint32_t pid;
    uint32_t level;
    char msg[BUF_SIZE_1K];
} daemon_log_slot_t;

typedef struct backups {
    char **files;
    int count;
    int cap;
} backups_t;

typedef struct daemon_logger {
    FILE *fp;                       // 현재 로그 파일 포인터
    char log_path[BUF_SIZE_256];    // 로그 파일 경로
    size_t max_log_size;            // 최대 로그 파일 크기 (byte)
    size_t curr_log_size;           // 현재 로그 파일 크기 (byte)
    int max_backup;                 // 최대 백업 파일 수 (0이면 백업 안함)
    bool debug;                     // 디버그 모드 플래그
    backups_t backups;
} daemon_logger_t;

// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================

static daemon_logger_t g_daemon_logger;

// ==== FUNCTION PROTOTYPES ===================================================

static int get_backup_log_files(const char *dirname, const char *filename, backups_t *backups);
static void cleanup_old_backup_log_files(void);
static int append_backup_log_file(const char *bak_log_path);
static int compare_log_files(const void *a, const void *b);
static int rotate_log(bool init);

// ==== FUNCTIONS =============================================================

int init_daemon_logger(const char *log_path, int max_log_size, int max_backup, bool debug)
{
    struct stat st;
    char path1[BUF_SIZE_256] = {0}, path2[BUF_SIZE_256] = {0};
    char *dir = NULL, *filename = NULL;

    if (log_path == NULL || *log_path == '\0') {
        return -1;
    }

    strncpy(path1, log_path, sizeof(path1) - 1);
    dir = dirname(path1);
    strncpy(path2, log_path, sizeof(path2) - 1);
    filename = basename(path2);
    
    // 로그 디렉터리 생성
    if (strcmp(dir, ".") != 0 && strcmp(dir, "/") != 0) {
        if (mkdir_p(dir, 0755) != 0) {
            return -1;
        }
    }
    
    strncpy(g_daemon_logger.log_path, log_path, sizeof(g_daemon_logger.log_path) - 1);
    g_daemon_logger.max_log_size = (max_log_size < 1) ? DEF_MAX_LOG_SIZE : (size_t)max_log_size * 1024 * 1024;
    g_daemon_logger.max_backup = (max_backup < 1) ? 0 : max_backup;
    g_daemon_logger.debug = debug;

    if (stat(log_path, &st) == 0) {
        // 로그 파일이 일반 파일이 아닌 경우 에러
        if (!S_ISREG(st.st_mode)) {
            return -1;
        }

        if ((size_t)st.st_size >= g_daemon_logger.max_log_size) {
            // 기존 로그 파일 크기가 최대 로그 크기 이상이면 로테이트
            if (rotate_log(true) != 0) {
                return -1;
            }
        }
        else {
            g_daemon_logger.curr_log_size = st.st_size;
        }
    }

    if (g_daemon_logger.max_backup > 0) {
        // 오름차순 정렬된 백업된 로그 파일 리스트 획득
        if (get_backup_log_files(dir, filename, &g_daemon_logger.backups) != 0) {
            return -1;
        }
        // 최대 백업 개수를 초과한 백업 파일들 삭제
        cleanup_old_backup_log_files();
    }
    
    return 0;
}

void destroy_daemon_logger(void)
{
    int i = 0;

    if (g_daemon_logger.fp != NULL) {
        fclose(g_daemon_logger.fp);
    }
    if (g_daemon_logger.backups.files != NULL) {
        for (i = 0; i < g_daemon_logger.backups.count; i++) {
            free(g_daemon_logger.backups.files[i]);
        }
        free(g_daemon_logger.backups.files);
    }
    memset(&g_daemon_logger, 0, sizeof(g_daemon_logger));
}

static int get_backup_log_files(const char *dirname, const char *filename, backups_t *backups)
{
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    const char *p = NULL;
    char **new_files = NULL;
    char path[BUF_SIZE_512] = {0};
    size_t len = 0, filename_len = 0, dirname_len = 0;
    int i = 0, new_cap = 0, ret = 0;
    bool valid = false;

    dir = opendir(dirname);
    if (dir == NULL) {
        return -1;
    }

    dirname_len = strlen(dirname);
    filename_len = strlen(filename);

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG) {
            continue;
        }

        len = strlen(entry->d_name);

        if (len != (filename_len + 15) ||
            strncmp(entry->d_name, filename, filename_len) != 0)
        {
            continue;
        }

        valid = true;
        for (p = entry->d_name + filename_len + 1; *p != '\0'; p++) {
            if (!isdigit(*p)) {
                valid = false;
                break;
            }
        }
        if (!valid) {
            continue;
        }

        if (backups->count == backups->cap) {
            new_cap = (backups->cap == 0) ? 16 : backups->cap * 2;
            new_files = (char **)realloc(backups->files, new_cap * sizeof(char *));
            if (new_files == NULL) {
                ret = -1;
                break;
            }
            backups->files = new_files;
            backups->cap = new_cap;
        }

        snprintf(path, sizeof(path), "%s%s%s", 
            dirname, 
            (dirname[dirname_len - 1] != '/') ? "/" : "",
            entry->d_name);
        backups->files[backups->count] = strdup(path);
        if (backups->files[backups->count] == NULL) {
            ret = -1;
            break;
        }
        backups->count++;
    }

    if (ret != 0) {
        if (backups->files != NULL) {
            for (i = 0; i < backups->count; i++) {
                free(backups->files[i]);
            }
            MEM_FREE(backups->files);
        }
        backups->count = 0;
        backups->cap = 0;
    }
    else {
        if (backups->count >= 2) {
            qsort(backups->files, (size_t)backups->count, sizeof(char *), compare_log_files);
        }
    }

    closedir(dir);

    return ret;
}

static void cleanup_old_backup_log_files(void)
{
    int rm_cnt = 0, i = 0;

    // 현재 백업 파일 수가 최대 백업 개수 이하이면 삭제할 필요 없음
    if (g_daemon_logger.backups.count <= g_daemon_logger.max_backup) {
        return;
    }

    // 삭제 대상 파일 개수 계산
    rm_cnt = g_daemon_logger.backups.count - g_daemon_logger.max_backup;

    // 오래된 파일부터 삭제 및 메모리 해제
    for (i = 0; i < rm_cnt; i++) {
        if (g_daemon_logger.backups.files[i] != NULL) {
            unlink(g_daemon_logger.backups.files[i]);
            MEM_FREE(g_daemon_logger.backups.files[i]);
        }
    }

    // 남은 백업 파일들을 배열 앞으로 당기고 개수 갱신
    memmove(&g_daemon_logger.backups.files[0], 
        &g_daemon_logger.backups.files[rm_cnt],
        g_daemon_logger.max_backup * sizeof(char *));
    g_daemon_logger.backups.count = g_daemon_logger.max_backup;
}

static int append_backup_log_file(const char *bak_log_path)
{
    int new_cap = 0;
    char **new_files = NULL;

    if (g_daemon_logger.backups.count == g_daemon_logger.backups.cap) {
        new_cap = (g_daemon_logger.backups.cap == 0) ? 16 : g_daemon_logger.backups.cap * 2;
        new_files = (char **)realloc(g_daemon_logger.backups.files, new_cap * sizeof(char *));
        if (new_files == NULL) {
            return -1;
        }
        g_daemon_logger.backups.files = new_files;
        g_daemon_logger.backups.cap = new_cap;
    }

    g_daemon_logger.backups.files[g_daemon_logger.backups.count] = strdup(bak_log_path);
    if (g_daemon_logger.backups.files[g_daemon_logger.backups.count] == NULL) {
        return -1;
    }
    g_daemon_logger.backups.count++;

    return 0;
}

static int compare_log_files(const void *a, const void *b)
{
    const char *file_a = *(const char **)a;
    const char *file_b = *(const char **)b;

    return strcmp(file_a, file_b);
}

static int rotate_log(bool init)
{
    struct tm tm_now;
    time_t now = 0;
    char time_str[BUF_SIZE_32] = {0}, path[BUF_SIZE_256 + BUF_SIZE_64] = {0};

    now = time(NULL);
    localtime_r(&now, &tm_now);
    strftime(time_str, sizeof(time_str), "%Y%m%d%H%M%S", &tm_now);

    snprintf(path, sizeof(path), "%s_%s", g_daemon_logger.log_path, time_str);

    if (rename(g_daemon_logger.log_path, path) != 0) {
        return -1;
    }

    if (!init && g_daemon_logger.max_backup > 0) {
        if (append_backup_log_file(path) != 0) {
            return -1;
        }
        cleanup_old_backup_log_files();
    }

    return 0;
}

