# MyShell — Layered Modular Monolithic Linux Shell

A modern, extensible, and modular shell built in C (C11/C17), inspired by modern interactive shells like **Fish** and structured according to the **Layered Modular Monolithic Architecture**.

---

## Architecture Overview

MyShell is structured into 5 decoupled layers:

```
+-----------------------------------------------------------------+
| Layer 1: User Interface (UI & Prompt)                           |
|   • Fish-style prompt (username, path formatting, exit code)    |
|   • ANSI color styling & Welcome banner                         |
|   • Session Command History                                     |
+-----------------------------------------------------------------+
| Layer 2: Language Processing                                    |
|   • Lexer / Tokenizer (handles spaces, single & double quotes)  |
|   • Parser / AST Builder (argv argument vector)                 |
+-----------------------------------------------------------------+
| Layer 3: Execution Engine                                       |
|   • Built-in Command Evaluator & Dispatcher                     |
|   • External Command Runner (fork & execvp / _spawnvp)          |
+-----------------------------------------------------------------+
| Layer 4: Runtime Management                                     |
|   • ShellState lifecycle (CWD tracking, history, exit codes)   |
|   • Environment variable propagation (PWD, OLDPWD)             |
+-----------------------------------------------------------------+
| Layer 5: OS Interface (POSIX / Linux / WSL2)                    |
|   • POSIX APIs (fork, exec, wait, stat, opendir)                |
+-----------------------------------------------------------------+
```

---

## Native Built-in Command Suite

MyShell contains its own handcrafted suite of native tools written from scratch:

| Command | Syntax | Description |
| :--- | :--- | :--- |
| **`ls`** | `ls [-l] [-a] [path]` | Custom colorized directory lister (folders in blue, executables in green, human-readable file sizes, permissions). |
| **`cat`** | `cat [-n] <file...>` | Native file viewer with optional line numbering. |
| **`sysinfo`** | `sysinfo` | Modern system dashboard (like `neofetch`/`fastfetch`) displaying OS, Arch, Host, Kernel, CWD, and color palette. |
| **`lscpu`** | `lscpu` | Native Linux kernel reader querying `/proc/cpuinfo` directly for model, MHz, cores, caches, and VT-x/AMD-V virtualization. |
| **`top`** | `top [-n iterations]` | Native live kernel process & memory monitor querying `/proc/stat`, `/proc/meminfo`, and `/proc/[pid]/`. |
| **`cd`** | `cd [dir \| ~ \| -]` | Directory switcher supporting home (`~`), previous directory (`-`), and relative/absolute paths. |
| **`pwd`** | `pwd` | Displays current working directory. |
| **`mkdir`** | `mkdir <dir...>` | Native directory creator. |
| **`touch`** | `touch <file...>` | Creates empty files or updates access timestamps. |
| **`nano`** | `nano [file]` | Native full-screen interactive terminal text editor (arrow keys, typing, `^O` save, `^X` exit). |
| **`echo`** | `echo [-n] [args...]` | Displays text, with `-n` to omit trailing newline. |
| **`clear`** | `clear` | Terminal screen clearer using ANSI escape codes. |
| **`history`** | `history` | Displays session history with line numbers. |
| **`type`** | `type <command>` | Inspects whether a command is a MyShell built-in or an external system binary. |
| **`help`** | `help` | Interactive help manual listing all built-in commands and usage. |
| **`exit`** | `exit [status]` | Cleanly exits the shell with optional exit code. |

---

## External Command Execution

Any command outside the native built-in list (such as `git`, `python`, `gcc`, `make`, `curl`, `gdb`, `vim`) is automatically resolved via `$PATH` and launched as a dedicated process using POSIX `fork()` and `execvp()`.

---

## Directory Structure

```text
myshell/
├── Makefile                # Automated build script
├── README.md               # Documentation & architectural manual
├── include/
│   ├── shell.h             # Shell state & lifecycle definitions
│   ├── ui.h                # Prompt rendering & ANSI color codes
│   ├── lexer.h             # Tokenizer interface
│   ├── parser.h            # Parser & command struct
│   ├── builtins.h          # Built-in command prototypes & registry
│   └── executor.h          # Execution engine interface
└── src/
    ├── main.c              # Interactive REPL loop
    ├── shell.c             # State & history management
    ├── ui.c                # Fish-style colored prompt & banner
    ├── lexer.c             # Tokenizer with quote preservation
    ├── parser.c            # Command argument vector parser
    ├── executor.c          # Evaluator & process dispatcher
    └── builtins/           # Native C implementations
        ├── builtin_core.c      # cd, pwd, echo, clear, history, type, help, exit
        ├── builtin_ls.c        # Custom colorized directory lister (-l, -a)
        ├── builtin_cat.c       # Custom file viewer (-n)
        ├── builtin_fileops.c   # mkdir, touch
        └── builtin_sysinfo.c   # Custom neofetch system dashboard
```

---

## Building & Running

### Using Make:
```bash
make
./myshell
```

### Direct GCC Compilation:
```bash
gcc -Wall -Wextra -std=c11 -Iinclude src/main.c src/shell.c src/ui.c src/lexer.c src/parser.c src/executor.c src/builtins/*.c -o myshell
./myshell
```
