CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -D_GNU_SOURCE
TARGET = myshell
BUILD_DIR = build

SRCS = src/main.c \
       src/shell.c \
       src/ui.c \
       src/lexer.c \
       src/parser.c \
       src/executor.c \
       src/scheduler.c \
       src/builtins/builtin_core.c \
       src/builtins/builtin_ls.c \
       src/builtins/builtin_cat.c \
       src/builtins/builtin_fileops.c \
       src/builtins/builtin_sysinfo.c \
       src/builtins/builtin_lscpu.c \
       src/builtins/builtin_top.c \
       src/builtins/builtin_ps.c \
       src/builtins/builtin_nano.c \
       src/builtins/builtin_sched.c \
       src/builtins/builtin_inspect.c

# Map each src/%.c to build/%.o
OBJS = $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(SRCS))

all: $(TARGET) workload

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

workload: benchmarks/workload.c
	$(CC) $(CFLAGS) -o workload benchmarks/workload.c

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(TARGET).exe workload workload.exe

.PHONY: all clean
