# Informations
VERSION			:= 1.0.0
GIT_HASH		:= $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")
BUILD_DATE		:= $(shell date +"%Y-%m-%d\ %H:%M:%S")
OS_ID			:= $(shell grep -w "ID" /etc/os-release | cut -d'=' -f2 | tr -d '"')
OS_VER			:= $(shell grep -w "VERSION_ID" /etc/os-release | cut -d'=' -f2 | tr -d '"')
BUILD_OS		:= $(OS_ID)$(OS_VER)
BUILD_KERN_VER	:= $(shell uname -r)
BUILD_ARCH		:= $(shell uname -m)

# Compiler & tools
CC		:= gcc
CLANG	:= clang
BPFTOOL	:= bpftool

# Flags
CFLAGS		:= -Wall -Wextra -O2 -Iinclude -Iskel
BPF_CFLAGS	:= -O2 -g -target bpf -Iinclude
LDFLAGS		:=
LDLIBS		:= -lpthread -lbpf

# Definitions
CFLAGS	+=	-DVERSION=\"$(VERSION)\"				\
			-DGIT_HASH=\"$(GIT_HASH)\"				\
			-DBUILD_DATE=\"$(BUILD_DATE)\"			\
			-DBUILD_OS=\"$(BUILD_OS)\"				\
			-DBUILD_KERN_VER=\"$(BUILD_KERN_VER)\"	\
			-DBUILD_ARCH=\"$(BUILD_ARCH)\"

# Directories
SRC_DIR		:= src
OBJ_DIR 	:= obj
BIN_DIR 	:= bin
BPF_DIR 	:= bpf
SKEL_DIR	:= skel

# Binary name
TARGET	:= $(BIN_DIR)/netto

# BPF object & skeleton header
BPF_SRC		:= $(BPF_DIR)/netto.bpf.c
BPF_OBJ		:= $(BIN_DIR)/netto.bpf.o
BPF_SKEL	:= $(SKEL_DIR)/netto.bpf.skel.h

# Auto-discover all user-space sources (recursive)
SRCS	:= $(shell find $(SRC_DIR) -name '*.c')

# Map src/**/*.c → obj/파일명.o (flat)
OBJS	:= $(patsubst %.c, $(OBJ_DIR)/%.o, $(notdir $(SRCS)))

# Dependency files
DEPS		:= $(OBJS:.o=.d)
BPF_DEPS	:= $(OBJ_DIR)/netto.bpf.d

# Register all source directories for vpath search
vpath %.c $(sort $(dir $(SRCS)))

# =============================================================================
.PHONY: all clean

all: $(TARGET)

# Link user-space binary
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Compile user-space objects (flat obj/)
$(OBJ_DIR)/%.o: %.c $(BPF_SKEL) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -MMD -MP -o $@ -c $<

# Compile BPF object
$(BPF_OBJ): $(BPF_SRC) | $(BIN_DIR) $(OBJ_DIR)
	$(CLANG) $(BPF_CFLAGS) -MMD -MP -MF $(BPF_DEPS) -o $@ -c $<

# Generate skeleton header from BPF object
$(BPF_SKEL): $(BPF_OBJ) | $(SKEL_DIR)
	$(BPFTOOL) gen skeleton $< > $@

# Create output directories
$(OBJ_DIR):
	mkdir -p $@

$(BIN_DIR):
	mkdir -p $@

$(SKEL_DIR):
	mkdir -p $@

# Pull in header dependencies
-include $(DEPS)
-include $(BPF_DEPS)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) $(SKEL_DIR)
