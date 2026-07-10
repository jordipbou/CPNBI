# CPNBI

**CPNBI** is a lightweight, cross-platform C library for non-blocking keyboard input handling in console applications.

Imitates Windows's conio.h kbhit() and getch() functions also for Linux (and maybe other platforms in the future).

## Features

- Non-blocking input detection
- Special key support (arrows, function keys, Home/End, Insert/Delete, Page Up/Down)
- Modifier key detection (Shift, Ctrl, Alt) with Kitty-compatible bit flags
- Two event modes: raw bytes or Unicode code points
- Cross-platform (Windows and Linux)
- Single-file library, no dependencies

## Building

### Without build system

Include the `cpnbi.c` and `cpnbi.h` files in your project. Then compile and link normally.

### Using CMake

Just add the cpnbi subdirectory and link to `cpnbi_lib`.

## Usage

Call `cpnbi_init()` once at startup to put the terminal in raw mode.
Then poll for events in a loop.

There are two ways to read events:

- **`cpnbi_get_event()`** — decodes escape sequences (arrows, function keys, etc.)
  but returns text as raw bytes (0–255). Use this when you want to handle
  character encoding yourself.

- **`cpnbi_get_unicode()`** — decodes escape sequences AND assembles multi-byte
  UTF-8 sequences into Unicode code points (0–0x10FFFF). Use this when your
  terminal is running in UTF-8 mode (the common case today).

```c
#include <stdio.h>
#include "cpnbi.h"

int main() {
    cpnbi_init();

    while (1) {
        if (cpnbi_is_event_available()) {
            int32_t e = cpnbi_get_unicode();

            if (cpnbi_event_is_special(e)) {
                printf("Special key: %d\n",
                       cpnbi_event_key(e));
            } else {
                printf("Unicode: U+%04X\n",
                       cpnbi_event_key(e));
            }

            int mod = cpnbi_event_mod(e);
            if (mod & CPNBI_MOD_SHIFT) printf("  Shift\n");
            if (mod & CPNBI_MOD_CTRL)  printf("  Ctrl\n");
            if (mod & CPNBI_MOD_ALT)   printf("  Alt\n");
        }
    }

    return 0;
}
```

On Windows both functions are identical — the Console API is Unicode-native
and always returns full Unicode code points (with surrogate pair reassembly
for characters above U+FFFF).

## API Reference

| Function | Description |
|---|---|
| `cpnbi_init()` | Initialize raw mode and signal handlers. Call once at startup. |
| `cpnbi_is_char_available()` | Non-blocking check if a raw byte is ready to read. |
| `cpnbi_is_event_available()` | Non-blocking check if an event is ready to read. |
| `cpnbi_get_char()` | Read one raw byte from stdin (blocking). Returns 0–255, or -1 on EOF. |
| `cpnbi_get_event()` | Read next event. Escape sequences are decoded into special keys; text bytes are returned individually (0–255). |
| `cpnbi_get_unicode()` | Read next event. Escape sequences are decoded; UTF-8 multi-byte sequences are assembled into Unicode code points. |
| `cpnbi_event_key(event)` | Extract the value field from a packed event (raw byte, Unicode code point, or special key). |
| `cpnbi_event_mod(event)` | Extract modifier flags from a packed event. |
| `cpnbi_event_type(event)` | Extract event type from a packed event (currently always 0 = press). |
| `cpnbi_event_is_special(event)` | Returns 1 if the value is a special key, 0 if it is text. |

## Event Format

The return value of `cpnbi_get_event()` and `cpnbi_get_unicode()` is a
packed 32-bit integer with the following layout:

```
bits 30-31: event type (reserved for press/repeat/release;
            always 0 = press for now)
bits 22-29: modifiers (8 Kitty-style flags)
bits  0-21: value (22 bits)
```

The value field holds:

- **`cpnbi_get_event()`**: 0–255 (raw byte) for text, or > 0x10FFFF for special keys
- **`cpnbi_get_unicode()`**: 0–0x10FFFF (Unicode code point) for text, or > 0x10FFFF for special keys

Use `cpnbi_event_is_special()` to distinguish text from special keys.

### Modifier Flags

| Constant | Bit | Meaning |
|---|---|---|
| `CPNBI_MOD_SHIFT` | 0 | Shift held |
| `CPNBI_MOD_ALT` | 1 | Alt held |
| `CPNBI_MOD_CTRL` | 2 | Ctrl held |
| `CPNBI_MOD_SUPER` | 3 | Super held |
| `CPNBI_MOD_HYPER` | 4 | Hyper held |
| `CPNBI_MOD_META` | 5 | Meta held |
| `CPNBI_MOD_CAPS_LOCK` | 6 | Caps Lock active |
| `CPNBI_MOD_NUM_LOCK` | 7 | Num Lock active |

### Special Keys

Special keys are returned as values above the Unicode range
(base `0x110000`). Available special keys:

- **Navigation**: `CPNBI_KEY_UP`, `CPNBI_KEY_DOWN`, `CPNBI_KEY_LEFT`, `CPNBI_KEY_RIGHT`, `CPNBI_KEY_HOME`, `CPNBI_KEY_END`, `CPNBI_KEY_PAGE_UP`, `CPNBI_KEY_PAGE_DOWN`
- **Editing**: `CPNBI_KEY_INSERT`, `CPNBI_KEY_DELETE`
- **Function keys**: `CPNBI_KEY_F1` through `CPNBI_KEY_F12`
- **Control**: `CPNBI_KEY_BACKSPACE`, `CPNBI_KEY_TAB`, `CPNBI_KEY_ENTER`, `CPNBI_KEY_ESCAPE`

## License

MIT License

## Interesting information about how terminals work

- ["The TTY demystified" by Linus Akesson](https://www.linusakesson.net/programming/tty/)
- ["A Brief Introduction to termios" by Nelson Elhage](https://blog.nelhage.com/2009/12/a-brief-introduction-to-termios/)
- ["Build Your Own Text Editor"](https://viewsourcecode.org/snaptoken/kilo/)
- [xterm's ctlseqs.txt](https://invisible-island.net/xterm/ctlseqs/ctlseqs.txt)
