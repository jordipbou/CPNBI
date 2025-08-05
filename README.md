# CPNBI

**CPNBI** is a lightweight, cross-platform C library for non-blocking keyboard input handling in console applications.

## Features

- Non-blocking input detection
- Support for special keys (arrows, function keys, etc.)
- Modifier key detection (Shift, Ctrl, Alt)
- Works on both Windows and Linux terminals
- Simple API

## Build

Include the `cpnbi.c` and `cpnbi.h` files in your project. Then compile and link normally.

## Usage

#include "cpnbi.h"

int main() {
    cpnbi_init();

    while (1) {
        if (cpnbi_is_event_available()) {
            int key = cpnbi_get_event();

            // You can decode the key and modifiers from the value
            // e.g., key % 1000 gives key code, key / 1000 gives modifiers
        }

        // do other things...
    }

    cpnbi_shutdown();
    return 0;
}

## License

MIT License
