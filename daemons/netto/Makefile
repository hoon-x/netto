# Compiler & tools
CLANG	:= clang
BPFTOOL	:= bpftool

# Directories
SRC_DIR		:= src
OBJ_DIR 	:= obj
INC_DIR		:= include
BPF_DIR 	:= bpf
SKEL_DIR	:= skel

# Flags
CFLAGS		+= -I$(INC_DIR) -I$(SKEL_DIR) -I$(COMMON_INC_DIR)
BPF_CFLAGS	:= -O2 -g -target bpf -I$(INC_DIR)
LDFLAGS		:=
LDLIBS		:= -lpthread -lbpf

# Definitions
CFLAGS	+=	-DVERSION=\"$(VERSION)\"				\
			-DGIT_HASH=\"$(GIT_HASH)\"				\
			-DBUILD_DATE=\"$(BUILD_DATE)\"			\
			-DBUILD_OS=\"$(BUILD_OS)\"				\
			-DBUILD_KERN_VER=\"$(BUILD_KERN_VER)\"	\
			-DBUILD_ARCH=\"$(BUILD_ARCH)\"

# Binary name
TARGET	:= netto

# BPF object & skeleton header
BPF_SRC		:= $(BPF_DIR)/netto.bpf.c
BPF_OBJ		:= $(OBJ_DIR)/netto.bpf.o
BPF_SKEL	:= $(SKEL_DIR)/netto.bpf.skel.h

# Auto-discover all user-space sources (recursive)
SRCS	:= $(shell find $(SRC_DIR) -name '*.c')
OBJS	:= $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

# Dependency files
DEPS		:= $(OBJS:.o=.d)
BPF_DEPS	:= $(OBJ_DIR)/netto.bpf.d

# =============================================================================
.PHONY: all clean install

all: $(TARGET)

# Link user-space binary
$(TARGET): $(OBJS) $(COMMON_LIB)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(COMMON_LIB) $(LDLIBS)

# Compile user-space objects
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(BPF_SKEL)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ -c $<

# Compile BPF object
$(BPF_OBJ): $(BPF_SRC) | $(OBJ_DIR)
	$(CLANG) $(BPF_CFLAGS) -MMD -MP -MF $(BPF_DEPS) -o $@ -c $<

# Generate skeleton header from BPF object
$(BPF_SKEL): $(BPF_OBJ) | $(SKEL_DIR)
	$(BPFTOOL) gen skeleton $< > $@

# Create output directories
$(OBJ_DIR):
	mkdir -p $@

$(SKEL_DIR):
	mkdir -p $@

install: all
	@cp $(TARGET) $(DEST_DIR)/
	@echo "    installed: $(TARGET) -> $(DEST_DIR)/"

# Pull in header dependencies
-include $(DEPS)
-include $(BPF_DEPS)

clean:
	rm -rf $(OBJ_DIR) $(SKEL_DIR) $(TARGET)
