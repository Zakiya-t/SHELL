CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude -D_POSIX_C_SOURCE=200809L
TARGET = myshell
BUILD_DIR = build

SRCS = src/main.c \
       src/shell.c \
       src/ui.c \
       src/lexer.c \
       src/parser.c \
       src/executor.c \
       src/builtins/builtin_core.c \
       src/builtins/builtin_ls.c \
       src/builtins/builtin_cat.c \
       src/builtins/builtin_fileops.c \
       src/builtins/builtin_sysinfo.c \
       src/builtins/builtin_lscpu.c \
       src/builtins/builtin_top.c \
       src/builtins/builtin_nano.c

# Map each src/%.c to build/%.o
OBJS = $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(TARGET).exe

.PHONY: all clean
