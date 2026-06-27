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
 * @file    config.c
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-06-20
 * @brief   설정 관련 기능 구현 소스 파일
 */

// ==== INCLUDES ==============================================================

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdalign.h>
#include <stddef.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

#include "config.h"
#include "macros.h"
#include "utils/event_count.h"
#include "utils/nt_string.h"

// ==== DEFINES / MACROS ======================================================
// ==== TYPEDEFS / STRUCTS ====================================================

typedef struct config_dbuf {
    config_t conf[2];
    pthread_mutex_t writer_lock;
    _Atomic int active_idx;
    alignas(CACHE_LINE_SIZE) _Atomic uint32_t refcnt[2];
    event_count_t ec;
} config_dbuf_t;

// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================

static config_dbuf_t g_conf_dbuf;

// ==== FUNCTION PROTOTYPES ===================================================

static void config_dump(config_t *cfg);
static int load_config(config_t *cfg, const char *conf_path, cfg_parser_fn parser_fn);

// ==== FUNCTIONS =============================================================

void runtime_config_init(runtime_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }

    atomic_init(&cfg->running, false);
}

void config_init(void)
{
    pthread_mutex_init(&g_conf_dbuf.writer_lock, NULL);
    atomic_init(&g_conf_dbuf.active_idx, 0);
    atomic_init(&g_conf_dbuf.refcnt[0], 0);
    atomic_init(&g_conf_dbuf.refcnt[1], 0);
    ec_init(&g_conf_dbuf.ec);
}

const config_t *config_acquire(void)
{
    while (true) {
        // 현재 활성화 상태의 이중버퍼 인덱스를 가져옴
        int idx = atomic_load_explicit(&g_conf_dbuf.active_idx, memory_order_relaxed);

        // 참조 카운트 증가
        atomic_fetch_add_explicit(&g_conf_dbuf.refcnt[idx], 1, memory_order_seq_cst);

        // 현재 활성 이중 버퍼 인덱스가 새로 가져온 인덱스와 일치한다면 설정 정보 반환
        if (atomic_load_explicit(&g_conf_dbuf.active_idx, memory_order_seq_cst) == idx) {
            return &g_conf_dbuf.conf[idx];
        }

        // 중간에 writer가 이중 버퍼 인덱스를 변경했다면 참조 카운트를 감소시키고 재시도
        uint32_t prev = atomic_fetch_sub_explicit(&g_conf_dbuf.refcnt[idx], 1, memory_order_release);
        if (prev == 1) {
            ec_notify(&g_conf_dbuf.ec);
        }
    }
}

void config_release(const config_t *cfg)
{
    assert(cfg == &g_conf_dbuf.conf[0] || cfg == &g_conf_dbuf.conf[1]);

    // 현재 설정 정보 구조체의 이중 버퍼 인덱스를 구함
    int idx = (int)(cfg - g_conf_dbuf.conf);

    // 참조 카운트를 감소시키고 내가 마지막 reader인 경우 혹시 잠들어 있을 writer를 깨워줌
    uint32_t prev = atomic_fetch_sub_explicit(&g_conf_dbuf.refcnt[idx], 1, memory_order_release);
    if (prev == 1) {
        ec_notify(&g_conf_dbuf.ec);
    }
}

int config_update(const char *conf_path, cfg_parser_fn parser_fn)
{
    if (conf_path == NULL || parser_fn == NULL) {
        return -1;
    }

    pthread_mutex_lock(&g_conf_dbuf.writer_lock);

    // 현재 비활성 상태의 이중 버퍼 인덱스를 구함
    int old_idx = atomic_load_explicit(&g_conf_dbuf.active_idx, memory_order_relaxed);
    int new_idx = old_idx ^ 1;

    // 설정 파일 로드
    if (load_config(&g_conf_dbuf.conf[new_idx], conf_path, parser_fn) != 0) {
        pthread_mutex_unlock(&g_conf_dbuf.writer_lock);
        return -1;
    }

    if (g_runconf.debug) {
        config_dump(&g_conf_dbuf.conf[new_idx]);
    }

    // 이중 버퍼 인덱스 swap
    atomic_store_explicit(&g_conf_dbuf.active_idx, new_idx, memory_order_seq_cst);

    // 이전 설정을 사용하던 모든 reader들이 작업을 끝마치길 기다림
    while (true) {
        ec_key_t key = ec_prepare_wait(&g_conf_dbuf.ec);

        if (atomic_load_explicit(&g_conf_dbuf.refcnt[old_idx], memory_order_seq_cst) == 0) {
            ec_cancel_wait(&g_conf_dbuf.ec);
            break;
        }

        ec_wait(&g_conf_dbuf.ec, key);
    }

    pthread_mutex_unlock(&g_conf_dbuf.writer_lock);

    return 0;
}

int netto_config_parser(config_t *cfg, const char *section, const char *key, const char *value)
{
    if (strcmp(section, "log") == 0) {
        if (strcmp(key, "max_log_size") == 0) {
            cfg->max_log_size = atoi(value);
        }
        else if (strcmp(key, "max_backup") == 0) {
            cfg->max_backup = atoi(value);
        }
    }

    return 0;
}

/**
 * @brief 설정 정보 덤프
 * 
 * @param cfg 설정 정보 구조체 포인터
 */
static void config_dump(config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }

    printf("==================================================\n");
    printf("max_log_size = %d\n", cfg->max_log_size);
    printf("max_backup = %d\n", cfg->max_backup);
    printf("==================================================\n");
}

/**
 * @brief 설정 파일 로드
 * 
 * @param cfg 설정 정보 구조체
 * @param conf_path 설정 파일 경로
 * @param parser_fn 파싱 콜백 함수
 * @return int 성공 시 0, 실패 시 -1
 */
static int load_config(config_t *cfg, const char *conf_path, cfg_parser_fn parser_fn)
{
    int ret = 0;

    FILE *fp = fopen(conf_path, "r");
    if (fp == NULL) {
        return -1;
    }

    char buf[BUF_SIZE_8K] = {0};
    char curr_section[BUF_SIZE_64] = {0};

    while (fgets(buf, sizeof(buf), fp) != NULL) {
        char *line = trim(buf);

        // 주석 및 빈 줄 무시
        if (*line == '\0' || *line == '#') {
            continue;
        }

        // 섹션 감지: [section]
        if (*line == '[') {
            char *end = strchr(line, ']');
            if (end != NULL) {
                *end = '\0';
                char *sec_name = trim(line + 1);
                strncpy(curr_section, sec_name, sizeof(curr_section) - 1);
            }
            continue;
        }

        // 섹션이 없는 key-value는 파싱하지 않음
        if (*curr_section == '\0') {
            continue;
        }

        // key-value 분리: key = value
        char *delim = strchr(line, '=');
        if (delim != NULL) {
            *delim = '\0';
            char *key = trim(line);
            char *value = trim(delim + 1);

            // key-value가 유효하면 파싱 함수 호출
            if (*key != '\0' && *value != '\0') {
                if (parser_fn(cfg, curr_section, key, value) != 0) {
                    ret = -1;
                    break;
                }
            }
        }
    }

    fclose(fp);

    return ret;
}
