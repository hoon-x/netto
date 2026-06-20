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
 * @file    process.c
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-05-05
 * @brief   프로세스 유틸리티 소스 파일
 */

// ==== INCLUDES ==============================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "utils/process.h"
#include "macros.h"

// ==== DEFINES / MACROS ======================================================
// ==== TYPEDEFS / STRUCTS ====================================================
// ==== GLOBAL VARIABLES ======================================================
// ==== STATIC VARIABLES ======================================================
// ==== FUNCTION PROTOTYPES ===================================================
// ==== FUNCTIONS =============================================================

bool is_process_running(pid_t pid)
{
    if (pid < 0) {
        return false;
    }

    // EPERM: 프로세스는 존재하나 권한이 없음
    if (kill(pid, 0) == 0 || errno == EPERM) {
        return true;
    }

    return false;
}

int get_procname_comm(pid_t pid, char *out, size_t out_size)
{
	char path[BUF_SIZE_128] = {0};
	FILE *fp = NULL;
	int ret = -1;

	if (pid <= 0 || out == NULL || out_size == 0) {
		return -1;
	}

	snprintf(path, sizeof(path), "/proc/%d/comm", (int)pid);

	fp = fopen(path, "r");
	if (fp == NULL) {
		return -1;
	}
	
	do {
		if (fgets(out, (int)out_size, fp) == NULL) {
			break;
		}
		out[strcspn(out, "\n")] = '\0';

		ret = 0;
	} while (0);
	
	fclose(fp);

	return ret;
}

int get_procname_cmdline(pid_t pid, char *out, size_t out_size, bool basename_only)
{
	char path[BUF_SIZE_128] = {0};
	char *base = NULL;
	FILE *fp = NULL;
	size_t len = 0;

	if (pid <= 0 || out == NULL || out_size == 0) {
		return -1;
	}

	snprintf(path, sizeof(path), "/proc/%d/cmdline", (int)pid);

	fp = fopen(path, "rb");
	if (fp == NULL) {
		return -1;
	}

	// cmdline의 인자는 '\0'로 구분되므로 첫 번째 토큰만 읽음
	len = fread(out, 1, out_size - 1, fp);
	fclose(fp);

	if (len == 0) {
		return -1;
	}

	out[len] = '\0';

	if (basename_only) {
		base = strrchr(out, '/');
		if (base != NULL) {
			memmove(out, base + 1, strlen(base));
		}
	}

	return 0;
}

int daemonize(void)
{
	pid_t pid = 0;
	int i = 0;

	pid = fork();
	if (pid < 0) {
		return -1;
	}
	else if (pid > 0) {
		// 부모 프로세스 종료 -> 데몬 프로세스가 백그라운드에서 실행됨
		exit(EXIT_SUCCESS);
	}

	if (setsid() < 0) {
		return -1;
	}

	pid = fork();
	if (pid < 0) {
		return -1;
	}
	else if (pid > 0) {
		// 첫 번째 자식 프로세스 종료 -> 최종 데몬 프로세스가 완전히 독립적으로 실행됨
		exit(EXIT_SUCCESS);
	}

	umask(0);

	// 열린 파일 디스크립터 모두 닫음
	for (i = 3; i < 1024; i++) {
		close(i);
	}

	return 0;
}

int redirect_devnull(void)
{
	int fd = -1, ret = -1;

	fd = open("/dev/null", O_RDWR);
	if (fd < 0) {
		return -1;
	}

	do {
		if (dup2(fd, STDIN_FILENO) < 0) {
			break;
		}
		if (dup2(fd, STDOUT_FILENO) < 0) {
			break;
		}
		if (dup2(fd, STDERR_FILENO) < 0) {
			break;
		}

		ret = 0;
	} while (0);

	// /dev/null 원본 FD가 0,1,2가 아니면 닫음
	if (fd > STDERR_FILENO) {
		close(fd);
	}

	return ret;
}
