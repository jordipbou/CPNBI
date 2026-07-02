# CPNBI

**CPNBI** is a lightweight, cross-platform C library for non-blocking keyboard input handling in console applications.

Imitates Windows's conio.h kbhit() and getch() functions also for Linux (and maybe other platforms in the future).

## Features

- Non-blocking input detection
- Support for special keys (arrows, function keys, etc.)
- Modifier key detection (Shift, Ctrl, Alt)
- Works on both Windows and Linux terminals
- Simple API

## Building

### Without build system

Include the `cpnbi.c` and `cpnbi.h` files in your project. Then compile and link normally.

### Using CMake

Just add the cpnbi subdirectory and link to `cpnbi_lib`.

## Usage

#include "cpnbi.h"

int main() {
cpnbi_init();

```
while (1) {
    if (cpnbi_is_event_available()) {
        int key = cpnbi_get_event();

        /* Decode key and modifiers using accessor functions:
					 cpnbi_event_key(event) returns the key,
					 cpnbi_event_mod(event) returns modifiers */
    }

    /* do other things... */
}

cpnbi_shutdown();
return 0;
```

}

## License

MIT License

## Interesting information about how terminals work

- ["The TTY demystified" by Linus Åkesson](https://www.linusakesson.net/programming/tty/)
- ["A Brief Introduction to termios" by Nelson Elhage](https://blog.nelhage.com/2009/12/a-brief-introduction-to-termios/)
- ["Build Your Own Text Editor"](https://viewsourcecode.org/snaptoken/kilo/)
- [xterm's ctlseqs.txt](https://invisible-island.net/xterm/ctlseqs/ctlseqs.txt)
