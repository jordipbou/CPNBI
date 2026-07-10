#ifndef CPNBI_H
#define CPNBI_H

#include <stdint.h>
#include <stdlib.h> /* atexit */

/* How long to wait for the rest of an escape sequence to 
	 arrive after seeing a lone ESC, before concluding it 
	 really was just the Esc key.
   25ms should be comfortably below "feels laggy to a human 
	 pressing Esc" and comfortably above realistic 
	 terminal-to-terminal latency (SSH, tmux, slow ptys) for 
	 the remaining 2-5 bytes of a sequence to arrive. */
#define CPNBI_ESCAPE_TIMEOUT_USEC 25000

/* Bit layout for cpnbi_get_event() return value:
   bits  0-21: value (22 bits)
               - 0 – 0x10FFFF: Unicode code point
               - > 0x10FFFF   : special key
   bits 22-29: modifiers (8 Kitty-style flags)
   bits 30-31: event type (reserved for press/repeat/release;
               always 0 = press for now) */

#define CPNBI_VALUE_SHIFT         0
#define CPNBI_VALUE_MASK          0x3FFFFF
#define CPNBI_MOD_OFFSET          22
#define CPNBI_MOD_MASK            0xFF
#define CPNBI_TYPE_OFFSET         30
#define CPNBI_TYPE_MASK           0x3

/* Modifier flags (Kitty-compatible bit positions) */

#define CPNBI_MOD_NONE            0
#define CPNBI_MOD_SHIFT           (1 << 0)
#define CPNBI_MOD_ALT             (1 << 1)
#define CPNBI_MOD_CTRL            (1 << 2)
#define CPNBI_MOD_SUPER           (1 << 3)
#define CPNBI_MOD_HYPER           (1 << 4)
#define CPNBI_MOD_META            (1 << 5)
#define CPNBI_MOD_CAPS_LOCK       (1 << 6)
#define CPNBI_MOD_NUM_LOCK        (1 << 7)

#define CPNBI_KEY_NUL             0
#define CPNBI_KEY_BACKSPACE       8
#define CPNBI_KEY_TAB             9
#define CPNBI_KEY_ENTER           13
#define CPNBI_KEY_ESCAPE          27

/* Special key base: placed above the Unicode range
   (max Unicode code point = 0x10FFFF = 1,114,111) */

#define CPNBI_KEY_BASE            0x110000

#define CPNBI_KEY_UP              (CPNBI_KEY_BASE + 256)
#define CPNBI_KEY_DOWN            (CPNBI_KEY_BASE + 257)
#define CPNBI_KEY_LEFT            (CPNBI_KEY_BASE + 258)
#define CPNBI_KEY_RIGHT           (CPNBI_KEY_BASE + 259)

#define CPNBI_KEY_HOME            (CPNBI_KEY_BASE + 260)
#define CPNBI_KEY_END             (CPNBI_KEY_BASE + 261)
#define CPNBI_KEY_INSERT          (CPNBI_KEY_BASE + 262)
#define CPNBI_KEY_DELETE          (CPNBI_KEY_BASE + 263)
#define CPNBI_KEY_PAGE_UP         (CPNBI_KEY_BASE + 264)
#define CPNBI_KEY_PAGE_DOWN       (CPNBI_KEY_BASE + 265)

#define CPNBI_KEY_F1              (CPNBI_KEY_BASE + 266)
#define CPNBI_KEY_F2              (CPNBI_KEY_BASE + 267)
#define CPNBI_KEY_F3              (CPNBI_KEY_BASE + 268)
#define CPNBI_KEY_F4              (CPNBI_KEY_BASE + 269)
#define CPNBI_KEY_F5              (CPNBI_KEY_BASE + 270)
#define CPNBI_KEY_F6              (CPNBI_KEY_BASE + 271)
#define CPNBI_KEY_F7              (CPNBI_KEY_BASE + 272)
#define CPNBI_KEY_F8              (CPNBI_KEY_BASE + 273)
#define CPNBI_KEY_F9              (CPNBI_KEY_BASE + 274)
#define CPNBI_KEY_F10             (CPNBI_KEY_BASE + 275)
#define CPNBI_KEY_F11             (CPNBI_KEY_BASE + 276)
#define CPNBI_KEY_F12             (CPNBI_KEY_BASE + 277)

void cpnbi_init();
int cpnbi_is_char_available();
int cpnbi_is_event_available();
int32_t cpnbi_get_char();
int32_t cpnbi_get_event();
int32_t cpnbi_event_key(int32_t event);
int32_t cpnbi_event_mod(int32_t event);
int32_t cpnbi_event_type(int32_t event);
int32_t cpnbi_event_is_special(int32_t event);

#endif
