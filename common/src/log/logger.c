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
#include "utils/event_count.h"
#include "utils/mpmc_queue.h"
#include "macros.h"

// ==== DEFINES / MACROS ======================================================

#define DEF_MAX_LOG_SIZE    (10 * 1024 * 1024)	// 10MB
#define LOG_QUEUE_SIZE      4096

// ==== TYPEDEFS / STRUCTS ====================================================

// Daemon 로그 슬롯 구조체
typedef struct daemon_log_slot {
    uint64_t timestamp_ns;
    uint32_t level;
    char msg[BUF_SIZE_1K];
} daemon_log_slot_t;

// 백업 로그 파일 리스트 구조체
typedef struct backups {
    char **files;
    int count;
    int cap;
} backups_t;

// 데몬 로거 구조체
typedef struct daemon_logger {
    FILE *fp;                       // 현재 로그 파일 포인터
    char log_path[BUF_SIZE_256];    // 로그 파일 경로
    size_t max_log_size;            // 최대 로그 파일 크기 (byte)
    size_t curr_log_size;           // 현재 로그 파일 크기 (byte)
    int max_backup;                 // 최대 백업 파일 수 (0이면 백업 안함)
    bool debug;                     // 디버그 모드 플래그
    bool enabled;                   // 로거 활성화 플래그
    pid_t pid;                      // 프로세스 ID
    backups_t backups;              // 백업 파일 리스트
    mpmc_queue_t *queue;            // Lock-Free MPMC Queue 구조체 포인터
    event_count_t ec;               // Lock-Free MPMC Queue 전용 Event Count 구조체
} daemon_logger_t;

// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================

static daemon_logger_t g_daemon_logger;

static const char *g_log_level_str[LEVEL_MAX] = {
    [LEVEL_DEBUG] = "[DEBUG]",
    [LEVEL_INFO] = "[INFO]",
    [LEVEL_WARN] = "[WARN]",
    [LEVEL_ERROR] = "[ERROR]",
    [LEVEL_FATAL] = "[FATAL]"
};

// ==== FUNCTION PROTOTYPES ===================================================

static int get_backup_log_files(const char *dirname, const char *filename, backups_t *backups);
static void cleanup_old_backup_log_files(void);
static int append_backup_log_file(const char *bak_log_path);
static int compare_log_files(const void *a, const void *b);
static int rotate_log(bool init);
static bool consume_one_daemon_log_entry(void);
static void write_daemon_log(const daemon_log_slot_t *slot);

// ==== FUNCTIONS =============================================================

int init_daemon_logger(const char *log_path, int max_log_size, int max_backup, bool debug)
{
    struct stat st;
    char path1[BUF_SIZE_256] = {0}, path2[BUF_SIZE_256] = {0};
    char *dir = NULL, *filename = NULL;
    int ret = -1;

    if (log_path == NULL || *log_path == '\0') {
        return -1;
    }

    // 디렉터리 및 파일명 추출
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

    do {
        // 로거 정보 저장
        strncpy(g_daemon_logger.log_path, log_path, sizeof(g_daemon_logger.log_path) - 1);
        g_daemon_logger.max_log_size = (max_log_size < 1) ? DEF_MAX_LOG_SIZE : (size_t)max_log_size * 1024 * 1024;
        g_daemon_logger.max_backup = (max_backup < 1) ? 0 : max_backup;
        g_daemon_logger.debug = debug;
        // Lock-Free Queue 생성
        g_daemon_logger.queue = create_mpmc_queue(LOG_QUEUE_SIZE, sizeof(daemon_log_slot_t));
        if (g_daemon_logger.queue == NULL) {
            break;
        }
        ec_init(&g_daemon_logger.ec);
        g_daemon_logger.pid = getpid();

        if (stat(log_path, &st) == 0) {
            // 로그 파일이 일반 파일이 아닌 경우 에러
            if (!S_ISREG(st.st_mode)) {
                break;
            }

            if ((size_t)st.st_size >= g_daemon_logger.max_log_size) {
                // 기존 로그 파일 크기가 최대 로그 크기 이상이면 로테이트
                if (rotate_log(true) != 0) {
                    break;
                }
            }
            else {
                g_daemon_logger.curr_log_size = st.st_size;
            }
        }

        if (g_daemon_logger.max_backup > 0) {
            // 오름차순 정렬된 백업된 로그 파일 리스트 획득
            if (get_backup_log_files(dir, filename, &g_daemon_logger.backups) != 0) {
                break;
            }
            // 최대 백업 개수를 초과한 백업 파일들 삭제
            cleanup_old_backup_log_files();
        }

        g_daemon_logger.fp = fopen(log_path, "a");
        if (g_daemon_logger.fp == NULL) {
            break;
        }

        g_daemon_logger.enabled = true;

        ret = 0;
    } while (0);

    if (ret != 0) {
        destroy_daemon_logger();
    }
    
    return ret;
}

void destroy_daemon_logger(void)
{
    int i = 0;

    FCLOSE(g_daemon_logger.fp);
    if (g_daemon_logger.backups.files != NULL) {
        for (i = 0; i < g_daemon_logger.backups.count; i++) {
            free(g_daemon_logger.backups.files[i]);
        }
        free(g_daemon_logger.backups.files);
    }
    destroy_mpmc_queue(g_daemon_logger.queue);
    memset(&g_daemon_logger, 0, sizeof(g_daemon_logger));
}

void *daemon_log_consumer_thread(thread_mgr_t *self)
{
    while (self->running) {
        // 우선 시도: 이미 쌓여있는 로그를 빠르게 처리하여 비움
        if (consume_one_daemon_log_entry()) {
            continue;
        }

        // epoch 캡쳐
        ec_key_t key = ec_prepare_wait(&g_daemon_logger.ec);

        // 재확인 - prepare_wait 이후 도착한 로그를 놓치지 않기 위함
        if (consume_one_daemon_log_entry()) {
            ec_cancel_wait(&g_daemon_logger.ec);
            continue;
        }

        if (!self->running) {
            ec_cancel_wait(&g_daemon_logger.ec);
            break;
        }

        ec_wait(&g_daemon_logger.ec, key);
    }

    // 종료 신호 이후에도 큐에 남은 메시지 모두 drain
    while (consume_one_daemon_log_entry()) {}

    return NULL;
}

void wake_daemon_log_consumer_thread(thread_mgr_t *self)
{
    (void)self;

    if (g_daemon_logger.enabled) {
        ec_notify_all(&g_daemon_logger.ec);
    }
}

void enqueue_daemon_log(log_level_t level, const char *fmt, ...)
{
    if (level >= LEVEL_MAX || fmt == NULL || !g_daemon_logger.enabled) {
        return;
    }

    // 디버그 모드가 아닌데 디버그 로그 레벨이면 무시
    if (!g_daemon_logger.debug && level == LEVEL_DEBUG) {
        return;
    }

    reserved_slot_t res;
    va_list args;
    
    // enqueue 슬롯 선점
    if (!reserve_enqueue_mpmc(g_daemon_logger.queue, &res)) {
        return;
    }

    // 로그 정보 설정
    daemon_log_slot_t *slot = (daemon_log_slot_t *)res.data;
    slot->timestamp_ns = get_now_time_ns();
    slot->level = level;

    va_start(args, fmt);
    vsnprintf(slot->msg, sizeof(slot->msg), fmt, args);
    va_end(args);

    // 선점한 슬롯 반환
    commit_enqueue_mpmc(g_daemon_logger.queue, &res);
    // 로그 수집 쓰레드가 sleep 상태인 경우 깨움
    ec_notify(&g_daemon_logger.ec);
}

/**
 * @brief 백업된 로그 파일 리스트를 가져오는 함수
 * 
 * @param dirname 디렉터리
 * @param filename 파일명
 * @param backups 백업 파일 리스트 구조체
 * @return int 성공 시 0, 실패 시 -1
 */
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

    // 로그 디렉터리 순회
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG) {
            continue;
        }

        len = strlen(entry->d_name);

        // 파일 유효성 체크
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
            // 수집한 파일들을 오름차순으로 정렬
            qsort(backups->files, (size_t)backups->count, sizeof(char *), compare_log_files);
        }
    }

    closedir(dir);

    return ret;
}

/**
 * @brief 최대 백업 개수를 초과하면 가장 오래된 로그 파일 삭제
 * 
 */
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

/**
 * @brief 백업 로그 파일 리스트에 새로운 파일 삽입
 * 
 * @param bak_log_path 백업 로그 파일 경로
 * @return int 성공 시 0, 실패 시 -1
 */
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

/**
 * @brief qsort()에서 사용할 로그 파일 경로 비교 콜백 함수
 * 
 * @param a 비교할 첫 번째 로그 파일 경로 포인터
 * @param b 비교할 두 번째 로그 파일 경로 포인터
 * @return int 문자열 비교 결과
 * @retval <0 첫 번째 파일 경로가 사전순으로 더 앞에 있음
 * @retval 0 두 파일 경로가 동일함
 * @retval >0 첫 번째 파일 경로가 사전순으로 더 뒤에 있음
 */
static int compare_log_files(const void *a, const void *b)
{
    const char *file_a = *(const char **)a;
    const char *file_b = *(const char **)b;

    return strcmp(file_a, file_b);
}

/**
 * @brief 로그 로테이트 함수
 * 
 * @param init 로거 초기화 플래그
 * @return int 성공 시 0, 실패 시 -1 
 */
static int rotate_log(bool init)
{
    struct tm tm_now;
    time_t now = 0;
    char time_str[BUF_SIZE_32] = {0}, path[BUF_SIZE_256 + BUF_SIZE_64] = {0};

    now = time(NULL);
    localtime_r(&now, &tm_now);
    strftime(time_str, sizeof(time_str), "%Y%m%d%H%M%S", &tm_now);

    snprintf(path, sizeof(path), "%s_%s", g_daemon_logger.log_path, time_str);

    // 로그 파일 백업
    if (rename(g_daemon_logger.log_path, path) != 0) {
        return -1;
    }
    g_daemon_logger.curr_log_size = 0;

    if (!init && g_daemon_logger.max_backup > 0) {
        // 최대 백업 개수가 있는 경우 파일 리스트에 삽입
        if (append_backup_log_file(path) != 0) {
            return -1;
        }
        // 최대 백업 개수를 넘어가는 경우 오래된 백업 파일 삭제
        cleanup_old_backup_log_files();
    }

    return 0;
}

/**
 * @brief 로그 큐에 삽입된 로그를 가져와 파일에 기록하는 함수
 * 
 * @return true 로그 처리 성공
 * @return false 큐가 비어있음
 */
static bool consume_one_daemon_log_entry(void)
{
    reserved_slot_t res;

    if (!reserve_dequeue_mpmc(g_daemon_logger.queue, &res)) {
        return false;
    }

    daemon_log_slot_t *slot = (daemon_log_slot_t *)res.data;
    write_daemon_log(slot);
    commit_dequeue_mpmc(g_daemon_logger.queue, &res);
    return true;
}

/**
 * @brief 로그를 파일에 기록하고, 필요 시 로그 로테이트하는 함수
 * 
 * @param slot 큐에서 가져온 슬롯 구조체 포인터
 */
static void write_daemon_log(const daemon_log_slot_t *slot)
{
    if (slot->timestamp_ns == 0 || slot->level >= LEVEL_MAX || *slot->msg == '\0') {
        return;
    }

    struct tm tm_now;
    char time_buf[BUF_SIZE_32] = {0};
    time_t sec = (time_t)(slot->timestamp_ns / 1000000000ULL);
    localtime_r(&sec, &tm_now);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_now);

    char buf[BUF_SIZE_2K] = {0};
    int len = snprintf(buf, sizeof(buf), "[%s][PID:%d]%s %s\n",
        time_buf, g_daemon_logger.pid, g_log_level_str[slot->level], slot->msg);

    // 로그 파일 사이즈가 최대 사이즈를 넘어가는 경우 로테이트
    if (g_daemon_logger.curr_log_size + len > g_daemon_logger.max_log_size) {
        FCLOSE(g_daemon_logger.fp);
        rotate_log(false);
    }

    if (g_daemon_logger.fp == NULL) {
        g_daemon_logger.fp = fopen(g_daemon_logger.log_path, "a");
        if (g_daemon_logger.fp == NULL) {
            return;
        }
    }

    size_t written = fwrite(buf, sizeof(char), len, g_daemon_logger.fp);
    g_daemon_logger.curr_log_size += written;

    if (g_daemon_logger.debug) {
        fprintf((slot->level < LEVEL_WARN) ? stdout : stderr, "%s", buf);
    }
}
