TARGET	:= libcommon.a

SRC_DIR	:= src
INC_DIR	:= include
OBJ_DIR	:= obj

CFLAGS	+= -I$(INC_DIR) $(EXTRA_CFLAGS)

SRCS	:= $(shell find $(SRC_DIR) -name '*.c')
OBJS	:= $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
DEPS	:= $(OBJS:.o=.d)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "--> Archiving $@"
	ar rcs $@ $(OBJS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "--> Compiling $<"
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

-include $(DEPS)
