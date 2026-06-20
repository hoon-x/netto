export VERSION			:= 1.0.0
export GIT_HASH			:= $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")
export BUILD_DATE		:= $(shell date +"%Y-%m-%d\ %H:%M:%S")
export OS_ID			:= $(shell grep -w "ID" /etc/os-release | cut -d'=' -f2 | tr -d '"')
export OS_VER			:= $(shell grep -w "VERSION_ID" /etc/os-release | cut -d'=' -f2 | tr -d '"')
export BUILD_OS			:= $(OS_ID)$(OS_VER)
export BUILD_KERN_VER	:= $(shell uname -r)
export BUILD_ARCH		:= $(shell uname -m)

export CC				:= gcc
export CFLAGS			?= -Wall -Wextra -O2 -MMD -MP

COMMON_DIR	:= common
DAEMONS_DIR	:= daemons
BIN_DIR		:= bin

export ROOT_DIR			:= $(CURDIR)
export COMMON_INC_DIR	:= $(ROOT_DIR)/$(COMMON_DIR)/include
export COMMON_LIB		:= $(ROOT_DIR)/$(COMMON_DIR)/libcommon.a

MAKEFLAGS				+= --no-print-directory

DAEMONS		:= netto

.PHONY: all clean release common collect $(DAEMONS)

all: $(DAEMONS) collect

common:
	@echo "========================================"
	@echo "--> Building Common Infrastructure..."
	@echo "========================================"
	$(MAKE) -C $(COMMON_DIR)

$(DAEMONS): common
	@echo "========================================"
	@echo "--> Building Daemon: $@"
	@echo "========================================"
	$(MAKE) -C $(DAEMONS_DIR)/$@

collect:
	@echo "========================================"
	@echo "--> Collecting Build Artifacts into $(BIN_DIR)/"
	@echo "========================================"
	@mkdir -p $(BIN_DIR)
	@for daemon in $(DAEMONS); do \
		$(MAKE) -C $(DAEMONS_DIR)/$$daemon install DEST_DIR=$(ROOT_DIR)/$(BIN_DIR); \
	done

release: clean all
	@echo "========================================"
	@echo "--> Release Build Complete"
	@echo "    VERSION:        $(VERSION)"
	@echo "    GIT_HASH:       $(GIT_HASH)"
	@echo "    BUILD_DATE:     $(BUILD_DATE)"
	@echo "    OS_ID:          $(OS_ID)"
	@echo "    OS_VER:         $(OS_VER)"
	@echo "    BUILD_OS:       $(BUILD_OS)"
	@echo "    BUILD_KERN_VER: $(BUILD_KERN_VER)"
	@echo "    BUILD_ARCH:     $(BUILD_ARCH)"
	@echo "========================================"

clean:
	@echo "--> Cleaning Common..."
	$(MAKE) -C $(COMMON_DIR) clean
	@for daemon in $(DAEMONS); do \
		echo "--> Cleaning Daemon: $$daemon"; \
		$(MAKE) -C $(DAEMONS_DIR)/$$daemon clean; \
	done
	@rm -rf $(BIN_DIR)
	@echo "--> Clean complete."
	