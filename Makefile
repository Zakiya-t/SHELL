CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude -D_POSIX_C_SOURCE=200809L
TARGET = myshell

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

OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET).exe

.PHONY: all clean
