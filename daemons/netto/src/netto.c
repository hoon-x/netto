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
 * @file    netto.c
 * @author  JongHoon Shim (shim9532@gmail.com)
 * @date    2026-05-04
 * @brief   Netto 메인 소스 파일
 */

// ==== INCLUDES ==============================================================

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "version.h"
#include "config.h"
#include "utils/process.h"
#include "log/logger.h"
#include "macros.h"

// ==== DEFINES / MACROS ======================================================
// ==== TYPEDEFS / STRUCTS ====================================================
// ==== GLOBAL VARIABLES ======================================================

runtime_config_t g_runconf;

// ==== STATIC VARIABLES ======================================================

static struct option g_options[] = {
    {"help",    no_argument,    0,  'h'},
    {"version", no_argument,    0,  'v'},
    {0,0,0,0}
};

static thread_mgr_t g_threads[] = {
	THREAD_ENTRY("nt_daemon_log", daemon_log_consumer_thread, NULL, wake_daemon_log_consumer_thread),
};

// ==== FUNCTION PROTOTYPES ===================================================

static void show_version(void);
static void show_usage(void);
static bool is_running(pid_t *pid);
static void sig_handler(int signum);
static int setup_signals(void);
static int write_pid_file(void);
static int chdir_to_executable(void);
static int initialize(bool debug);
static void finalize(void);

// ==== FUNCTIONS =============================================================

/**
 * @brief Netto 메인 함수
 * 
 * @param argc 인자값 개수
 * @param argv 인자값
 * @return int - 정상 종료 시 0, 비정상 종료 시 exit code
 */
int main(int argc, char *argv[])
{
    int opt = 0, opt_idx = 0;
    char *cmd = NULL;
    bool is_start = false, is_stop = false, is_debug = false;
	pid_t pid = 0;

    if (argc <= 1) {
        show_usage();
        exit(EXIT_SUCCESS);
    }

	if (chdir_to_executable() != 0) {
		fprintf(stderr, "[ERROR] Failed to change directory to executable\n");
		exit(EXIT_FAILURE);
	}

    while ((opt = getopt_long(argc, argv, "hv", g_options, &opt_idx)) != -1) {
        switch (opt) {
			case 'h':
				show_usage();
				exit(EXIT_SUCCESS);
			case 'v':
				show_version();
				exit(EXIT_SUCCESS);
			default:
				break;
        }
    }

    if (optind < argc) {
        cmd = argv[optind];
        is_start = (strcmp(cmd, "start") == 0);
        is_stop = (strcmp(cmd, "stop") == 0);
        is_debug = (strcmp(cmd, "debug") == 0);
    }

    if (is_start || is_debug) {
		if (is_running(&pid)) {
			fprintf(stderr, "[WARN] %s is already running (PID: %d)\n", NETTO_NAME, (int)pid);
			exit(EXIT_SUCCESS);
		}
    }
    else if (is_stop) {
		if (is_running(&pid)) {
			if (kill(pid, SIGTERM) != 0) {
				fprintf(stderr, "[ERROR] Failed to stop %s (PID: %d)\n", NETTO_NAME, (int)pid);
				exit(EXIT_FAILURE);
			}
			else {
				printf("[INFO] %s (PID: %d) is stopping...\n", NETTO_NAME, (int)pid);
			}
		}
		exit(EXIT_SUCCESS);
    }
    else {
        show_usage();
        exit(EXIT_SUCCESS);
    }

	if (is_start) {
		if (daemonize() != 0) {
			fprintf(stderr, "[ERROR] Failed to daemonize %s\n", NETTO_NAME);
			exit(EXIT_FAILURE);
		}
	}

	if (initialize(is_debug) != 0) {
		exit(EXIT_FAILURE);
	}

	LOG_INFO("Run %s (mode:%s)", NETTO_NAME, is_debug ? "debug" : "normal");

	while (atomic_load_explicit(&g_runconf.running, memory_order_relaxed)) {
		sleep(1);
	}

	LOG_INFO("Shutdown %s", NETTO_NAME);

	finalize();

    return 0;
}

/**
 * @brief 버전 정보 출력
 * 
 */
static void show_version(void)
{
    printf("================================================================================\n");
    printf("%s Version: %s, Git Hash: %s, Build Date: %s\n",
        NETTO_NAME, VERSION, GIT_HASH, BUILD_DATE);
    printf("Build OS: %s, Build Kern Ver: %s, Build Arch: %s\n",
        BUILD_OS, BUILD_KERN_VER, BUILD_ARCH);
    printf("================================================================================\n");
}

/**
 * @brief 사용법 출력
 * 
 */
static void show_usage(void)
{
    printf("================================================================================\n");
    printf("Usage: %s [command] [options]\n", NETTO_NAME);
    printf("Command:\n");
    printf("  start       Run %s as a daemon (background)\n", NETTO_NAME);
    printf("  stop        Shutdown the running %s daemon\n", NETTO_NAME);
    printf("  debug       Run %s in the foreground (debug mode)\n", NETTO_NAME);
    printf("Options:\n");
    printf("  -h, --help        Show help message\n");
    printf("  -v, --version     Show version information\n");
    printf("================================================================================\n");
}

/**
 * @brief PID 파일을 읽어서 프로세스가 실행 중인지 확인하는 함수
 * 
 * @param pid PID 파일에서 읽어온 프로세스 ID가 저장될 포인터
 * @return bool - true(실행 중), false(종료됨 또는 존재하지 않음)
 */
static bool is_running(pid_t *pid)
{
	FILE *fp = NULL;
	char proc_name[BUF_SIZE_64] = {0};
	bool running = false;

	if (pid == NULL) {
		return false;
	}

	fp = fopen(PID_PATH, "r");
	if (fp == NULL) {
		return false;
	}

	do {
		if (fscanf(fp, "%d", pid) != 1) {
			break;
		}

		if (get_procname_comm(*pid, proc_name, sizeof(proc_name)) != 0) {
			break;
		}

		if (strcmp(proc_name, NETTO_NAME) != 0) {
			break;
		}

		running = true;
	} while (0);

	fclose(fp);
	return running;
}

/**
 * @brief 시그널 핸들러 함수
 * 
 * @param signum 시그널 번호
 */
static void sig_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		atomic_store_explicit(&g_runconf.running, false, memory_order_relaxed);
	}
}

/**
 * @brief 시그널 핸들러 등록 함수
 * 
 * @return int - 성공 시 0, 실패 시 -1
 */
static int setup_signals(void)
{
	struct sigaction sa;
	int i = 0;
	const int notify_signals[] = {
		SIGINT,		// Ctrl+C
		SIGTERM		// 시스템 종료 시그널
	};
	const int ignore_signals[] = {
		SIGHUP,		// 터미널 종료
        SIGPIPE,	// 끊긴 파이프 쓰기    
        SIGQUIT,	// Ctrl+\, 코어 덤프
        SIGTSTP,	// Ctrl+Z, 터미널 정지
        SIGTTIN,	// 백그라운드 터미널 읽기
        SIGTTOU,	// 백그라운드 터미널 쓰기
        SIGALRM,	// alarm() 타이머 
        SIGUSR1,	// 사용자 정의 1  
        SIGUSR2,	// 사용자 정의 2   
        SIGCHLD,	// 자식 종료 (직접 waitpid로 회수)
        SIGCONT,	// 정지 후 재개   
        SIGURG,		// 소켓 긴급 데이터  
        SIGXCPU,	// CPU 시간 초과
        SIGXFSZ,	// 파일 크기 초과
        SIGVTALRM,	// 가상 타이머
        SIGPROF,	// 프로파일링 타이머
        SIGWINCH,	// 터미널 크기 변경
        SIGIO,		// 비동기 I/O
        SIGPWR,		// 전원 이상
        SIGSYS,		// 잘못된 시스템 콜
        SIGBUS		// 버스 오류 (하드웨어)
	};

	memset(&sa, 0, sizeof(sa));
	sigemptyset(&sa.sa_mask);

	// 종료 시그널은 핸들러 등록
	sa.sa_flags = 0;
	sa.sa_handler = sig_handler;
	for (i = 0; i < (int)ARRAY_SIZE(notify_signals); i++) {
		if (sigaction(notify_signals[i], &sa, NULL) != 0) {
			return -1;
		}
	}

	// 무시할 시그널은 핸들러를 SIG_IGN으로 등록
	sa.sa_flags = 0;
	sa.sa_handler = SIG_IGN;
	for (i = 0; i < (int)ARRAY_SIZE(ignore_signals); i++) {
		if (sigaction(ignore_signals[i], &sa, NULL) != 0) {
			return -1;
		}
	}

	return 0;
}

/**
 * @brief PID 파일에 현재 프로세스 ID를 기록하는 함수
 * 
 * @return int 성공 시 0, 실패 시 -1
 */
static int write_pid_file(void)
{
	FILE *fp = NULL;

	if (mkdir(VAR_DIR, 0755) != 0 && errno != EEXIST) {
		return -1;
	}

	fp = fopen(PID_PATH, "w");
	if (fp == NULL) {
		return -1;
	}

	fprintf(fp, "%d\n", (int)getpid());
	fclose(fp);

	return 0;
}

/**
 * @brief 실행 파일이 위치한 디렉토리로 현재 작업 디렉토리를 변경하는 함수
 * 
 * @return int 성공 시 0, 실패 시 -1
 */
static int chdir_to_executable(void)
{
	char exe_path[BUF_SIZE_256] = {0};

	if (readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1) <= 0) {
		return -1;
	}

	char *dir = dirname(exe_path);
	if (chdir(dir) != 0) {
		return -1;
	}

	return 0;
}

/**
 * @brief 초기화 함수
 * 
 * @param debug 디버그 모드 여부
 * 
 * @return int 성공 시 0, 실패 시 -1
 */
static int initialize(bool debug)
{
	int ret = -1;

	runtime_config_init(&g_runconf);
	atomic_store_explicit(&g_runconf.running, true, memory_order_relaxed);

	// 자신의 PID를 파일에 기록
	if (write_pid_file() != 0) {
		fprintf(stderr, "[ERROR] Failed to write PID file\n");
		return -1;
	}

	// 시그널 핸들링 설정
	if (setup_signals() != 0) {
		fprintf(stderr, "[ERROR] Failed to setup signal handlers\n");
		return -1;
	}

	// 설정 정보 관리 구조체 초기화
	config_init();

	// 데몬 로거 초기화
	if (init_daemon_logger(LOG_PATH, 10, 0, debug) != 0) {
		fprintf(stderr, "[ERROR] Failed to initialize daemon logger\n");
		return -1;
	}

	do {
		init_thread_mgr(g_threads, ARRAY_SIZE(g_threads));

		// 등록된 모든 서비스 쓰레드 가동
		if (start_thread_mgr_all(g_threads, ARRAY_SIZE(g_threads)) != 0) {
			fprintf(stderr, "[ERROR] Failed to start threads\n");
			break;
		}

		ret = 0;
	} while (0);
	
	if (ret != 0) {
		destroy_daemon_logger();
	}

	return 0;
}

/**
 * @brief 종료 함수
 */
static void finalize(void)
{
	// 가동 중인 모든 서비스 쓰레드 종료
	stop_thread_mgr_all(g_threads, ARRAY_SIZE(g_threads));

	// 데몬 로거 해제
	destroy_daemon_logger();
}
