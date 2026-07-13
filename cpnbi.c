#include <stdio.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <conio.h>
#include <windows.h>
#include "cpnbi.h"

static HANDLE hStdin;
static DWORD orig_mode;
static int cpnbi__is_console;
static int cpnbi__is_pipe;
static int cpnbi__eof;
static int cpnbi__pushback = -1;

/* Forward declaration - cpnbi__decode_event() is defined once,
   after the platform #endif, and is shared by both builds. */
int32_t cpnbi__decode_event(int (*next_byte)(void),
                            int (*more_available)(void),
                            int utf8);

void
cpnbi__shutdown() {
	if (cpnbi__is_console) {
		SetConsoleMode(hStdin, orig_mode);
	}
}

BOOL WINAPI
cpnbi__ctrl_handler(DWORD ctrl_type) {
	switch (ctrl_type) {
		case CTRL_C_EVENT:
		case CTRL_BREAK_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT: cpnbi__shutdown(); break;
	}
	return FALSE;
}

void
cpnbi_init() {
	hStdin = GetStdHandle(STD_INPUT_HANDLE);

	/* A real console input handle answers GetConsoleMode.
	   A redirected pipe or file does not - in that case we
	   read bytes directly through cpnbi__decode_event() and
	   never touch console mode. */
	if (GetConsoleMode(hStdin, &orig_mode)) {
		DWORD mode = 0;

		cpnbi__is_console = 1;
		mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
		mode |= ENABLE_WINDOW_INPUT | ENABLE_PROCESSED_INPUT;
		SetConsoleMode(hStdin, mode);

		atexit(cpnbi__shutdown);
		SetConsoleCtrlHandler(cpnbi__ctrl_handler, TRUE);
	} else {
		cpnbi__is_console = 0;
		cpnbi__eof = 0;
		cpnbi__pushback = -1;
		cpnbi__is_pipe =
		    (GetFileType(hStdin) == FILE_TYPE_PIPE);
		if (cpnbi__is_pipe) {
			DWORD nowait = PIPE_NOWAIT;

			SetNamedPipeHandleState(hStdin, &nowait,
			                        NULL, NULL);
		}
	}
}

/* Process a single INPUT_RECORD and return a packed event
   using the new bit layout. Does NOT handle surrogate pair
   assembly — that is done in cpnbi_get_event(). */
int32_t
cpnbi__process_event(INPUT_RECORD* record) {
	int mod = CPNBI_MOD_NONE;

	if (record->EventType != KEY_EVENT
	    || !record->Event.KeyEvent.bKeyDown) {
		return 0;
	}

	WORD vk = record->Event.KeyEvent.wVirtualKeyCode;
	DWORD ctrl = record->Event.KeyEvent.dwControlKeyState;

	if (ctrl & SHIFT_PRESSED) {
		mod |= CPNBI_MOD_SHIFT;
	}
	if (ctrl & LEFT_CTRL_PRESSED
	    || ctrl & RIGHT_CTRL_PRESSED) {
		mod |= CPNBI_MOD_CTRL;
	}
	if (ctrl & LEFT_ALT_PRESSED || ctrl & RIGHT_ALT_PRESSED) {
		mod |= CPNBI_MOD_ALT;
	}

	switch (vk) {
		case VK_ESCAPE:
			return CPNBI_KEY_ESCAPE | (mod << CPNBI_MOD_OFFSET);
		case VK_RETURN:
			return CPNBI_KEY_ENTER | (mod << CPNBI_MOD_OFFSET);
		case VK_BACK:
			return CPNBI_KEY_BACKSPACE
			       | (mod << CPNBI_MOD_OFFSET);
		case VK_TAB:
			return CPNBI_KEY_TAB | (mod << CPNBI_MOD_OFFSET);
		case VK_UP:
			return CPNBI_KEY_UP | (mod << CPNBI_MOD_OFFSET);
		case VK_DOWN:
			return CPNBI_KEY_DOWN | (mod << CPNBI_MOD_OFFSET);
		case VK_LEFT:
			return CPNBI_KEY_LEFT | (mod << CPNBI_MOD_OFFSET);
		case VK_RIGHT:
			return CPNBI_KEY_RIGHT | (mod << CPNBI_MOD_OFFSET);
		case VK_HOME:
			return CPNBI_KEY_HOME | (mod << CPNBI_MOD_OFFSET);
		case VK_END:
			return CPNBI_KEY_END | (mod << CPNBI_MOD_OFFSET);
		case VK_INSERT:
			return CPNBI_KEY_INSERT | (mod << CPNBI_MOD_OFFSET);
		case VK_DELETE:
			return CPNBI_KEY_DELETE | (mod << CPNBI_MOD_OFFSET);
		case VK_PRIOR:
			return CPNBI_KEY_PAGE_UP | (mod << CPNBI_MOD_OFFSET);
		case VK_NEXT:
			return CPNBI_KEY_PAGE_DOWN
			       | (mod << CPNBI_MOD_OFFSET);
		case VK_F1:
			return CPNBI_KEY_F1 | (mod << CPNBI_MOD_OFFSET);
		case VK_F2:
			return CPNBI_KEY_F2 | (mod << CPNBI_MOD_OFFSET);
		case VK_F3:
			return CPNBI_KEY_F3 | (mod << CPNBI_MOD_OFFSET);
		case VK_F4:
			return CPNBI_KEY_F4 | (mod << CPNBI_MOD_OFFSET);
		case VK_F5:
			return CPNBI_KEY_F5 | (mod << CPNBI_MOD_OFFSET);
		case VK_F6:
			return CPNBI_KEY_F6 | (mod << CPNBI_MOD_OFFSET);
		case VK_F7:
			return CPNBI_KEY_F7 | (mod << CPNBI_MOD_OFFSET);
		case VK_F8:
			return CPNBI_KEY_F8 | (mod << CPNBI_MOD_OFFSET);
		case VK_F9:
			return CPNBI_KEY_F9 | (mod << CPNBI_MOD_OFFSET);
		case VK_F10:
			return CPNBI_KEY_F10 | (mod << CPNBI_MOD_OFFSET);
		case VK_F11:
			return CPNBI_KEY_F11 | (mod << CPNBI_MOD_OFFSET);
		case VK_F12:
			return CPNBI_KEY_F12 | (mod << CPNBI_MOD_OFFSET);
		default: {
			WCHAR uc = record->Event.KeyEvent.uChar.UnicodeChar;
			if (uc != 0) {
				return (int32_t)uc | (mod << CPNBI_MOD_OFFSET);
			}
		}
	}

	return 0;
}

/* --- Redirected (pipe/file) input primitives --- */
/* These feed cpnbi__decode_event() the same way the Linux
   implementation feeds it: one raw byte at a time, with a
   non-consuming peek for lone-Esc disambiguation. A pipe is
   put in PIPE_NOWAIT so a read that would block returns
   immediately (EAGAIN) instead of stalling. */

static int
cpnbi__win_try_read(unsigned char* out) {
	/* 1 = byte read, 0 = EOF, -1 = would block */
	DWORD n = 0;

	if (ReadFile(hStdin, out, 1, &n, NULL)) {
		if (n == 1) {
			return 1;
		}
		cpnbi__eof = 1;
		return 0;
	}
	if (GetLastError() == ERROR_NO_DATA) {
		return -1;
	}
	cpnbi__eof = 1;
	return 0;
}

static int
cpnbi__win_next_byte(void) {
	unsigned char b;

	if (cpnbi__pushback >= 0) {
		int ch = cpnbi__pushback;

		cpnbi__pushback = -1;
		return ch;
	}

	if (cpnbi__win_try_read(&b) == 1) {
		return b;
	}
	return CPNBI_EOF;
}

/* Non-consuming peek: report 1 and leave a byte in
   cpnbi__pushback when data is immediately available; 0 if a
   read would block (live, empty pipe) or the stream has ended
   (cpnbi__win_byte_available() turns the EOF case into
   "ready" so callers read once more).
   A pipe is read in non-blocking mode (PIPE_NOWAIT); a
   regular redirected file is read directly (it never blocks). */
static int
cpnbi__win_peek(void) {
	unsigned char b;
	DWORD n = 0;

	if (cpnbi__pushback >= 0) {
		return 1;
	}

	if (cpnbi__is_pipe) {
		switch (cpnbi__win_try_read(&b)) {
			case 1:
				cpnbi__pushback = b;
				return 1;
			case -1:
				return 0; /* empty but open: would block */
			default:
				cpnbi__eof = 1; /* closed pipe => EOF */
				return 0;
		}
	}

	if (ReadFile(hStdin, &b, 1, &n, NULL) && n == 1) {
		cpnbi__pushback = b;
		return 1;
	}
	cpnbi__eof = 1;
	return 0;
}

static int
cpnbi__win_more_available(void) {
	return cpnbi__win_peek();
}

static int
cpnbi__win_byte_available(void) {
	if (cpnbi__eof) {
		return 1;
	}
	cpnbi__win_peek();
	return cpnbi__eof ? 1 : (cpnbi__pushback >= 0);
}

int
cpnbi_is_byte_available() {
	if (cpnbi__is_console) {
		return _kbhit();
	}
	return cpnbi__win_byte_available();
}

int
cpnbi_is_event_available() {
	if (cpnbi__is_console) {
		DWORD count;
		INPUT_RECORD record;

		PeekConsoleInput(hStdin, &record, 1, &count);

		if (count > 0) {
			int32_t res = cpnbi__process_event(&record);

			if (res != 0) {
				return 1;
			} else {
				ReadConsoleInput(hStdin, &record, 1,
				                &count);
			}
		}

		return 0;
	}
	return cpnbi__win_byte_available();
}

/* Raw byte read — the building block.
   Returns 0-255 on success. Blocking. */
int32_t
cpnbi_get_byte() {
	if (cpnbi__is_console) {
		return _getch();
	}
	return cpnbi__win_next_byte();
}

/* Decoded event: handles VK codes on the Windows console
   API, plus surrogate pair reassembly for characters above
   U+FFFF. Returns a packed int32_t per the CPNBI bit layout. */
int32_t
cpnbi_get_event() {
	if (cpnbi__is_console) {
		DWORD count;
		INPUT_RECORD record;
		int32_t pending_high = 0;

		while (1) {
			ReadConsoleInput(hStdin, &record, 1, &count);
			if (count == 0) {
				continue;
			}

			int32_t res = cpnbi__process_event(&record);
			if (res == 0) {
				continue;
			}

			int32_t key = res & CPNBI_VALUE_MASK;

			if (pending_high) {
				if (key >= 0xDC00
				    && key <= 0xDFFF) {
					int32_t cp = 0x10000
					             + (pending_high
					                - 0xD800) * 0x400
					             + (key - 0xDC00);
					pending_high = 0;
					int32_t mod =
					    (res >> CPNBI_MOD_OFFSET)
					    & CPNBI_MOD_MASK;
					return cp
					       | (mod << CPNBI_MOD_OFFSET);
				}
				pending_high = 0;
			}

			if ((key & CPNBI_VALUE_MASK) >= 0xD800
			    && (key & CPNBI_VALUE_MASK)
			           <= 0xDBFF) {
				pending_high = key;
				continue;
			}

			return res;
		}
	}
	return cpnbi__decode_event(cpnbi__win_next_byte,
	                           cpnbi__win_more_available, 0);
}

int32_t
cpnbi_get_unicode() {
	if (cpnbi__is_console) {
		return cpnbi_get_event();
	}
	return cpnbi__decode_event(cpnbi__win_next_byte,
	                           cpnbi__win_more_available, 1);
}

#else

/* LINUX IMPLEMENTATION */

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#include "cpnbi.h"

static struct termios orig_termios;
static int cpnbi__is_tty;
static int cpnbi__eof;

void
cpnbi__shutdown() {
	if (cpnbi__is_tty) {
		tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
	}
}

static void
cpnbi__signal_handler(int signum) {
	cpnbi__shutdown();
	signal(signum, SIG_DFL);
	raise(signum);
}

void
cpnbi_init() {
	cpnbi__eof = 0;
	cpnbi__is_tty = isatty(STDIN_FILENO);

	if (!cpnbi__is_tty) {
		return;
	}

	{
		struct termios raw;

		tcgetattr(STDIN_FILENO, &orig_termios);
		raw = orig_termios;
		raw.c_lflag &= ~(ICANON | ECHO);
		raw.c_cc[VMIN] = 1;
		raw.c_cc[VTIME] = 0;
		tcsetattr(STDIN_FILENO, TCSANOW, &raw);

		atexit(cpnbi__shutdown);
		signal(SIGINT, cpnbi__signal_handler);
		signal(SIGTERM, cpnbi__signal_handler);
	}
}

/* Raw byte read from stdin. Blocking.
   Returns 0-255 on success, CPNBI_EOF on end of stream. */
int32_t
cpnbi_get_byte() {
	int ch = getchar();

	if (ch == EOF) {
		cpnbi__eof = 1;
		return CPNBI_EOF;
	}

	return (int32_t)ch;
}

int32_t
cpnbi__getch() {
	return cpnbi_get_byte();
}

/* Returns 1 if a real byte can be read now without
   blocking, 0 if a read would block, -1 at end-of-stream.
   Sets cpnbi__eof on the EOF path. */
static int
cpnbi__data_available(void) {
	int ch;
	int oldf;

	oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
	ch = getchar();
	fcntl(STDIN_FILENO, F_SETFL, oldf);

	if (ch == EOF) {
		if (feof(stdin)) {
			cpnbi__eof = 1;
			return -1;
		}
		return 0;
	}

	ungetc(ch, stdin);
	return 1;
}

/* Public availability: 1 if a read will not block, which
   includes end-of-stream (get_*() then returns CPNBI_EOF). */
static int
cpnbi__byte_available(void) {
	return cpnbi__data_available() != 0 || cpnbi__eof;
}

int
cpnbi_is_byte_available(void) {
	return cpnbi__byte_available();
}

int
cpnbi_is_event_available(void) {
	return cpnbi__byte_available();
}

/* Forward declarations */
int32_t cpnbi__decode_event(int (*next_byte)(void),
                            int (*more_available)(void),
                            int utf8);
static int cpnbi__escape_followup_available(void);

static int
cpnbi__escape_followup_available(void) {
	int r = cpnbi__data_available();

	if (r > 0) {
		return 1;
	}

	if (r < 0) {
		return 0;
	}

	/* No byte available yet: wait briefly for the rest of an
	   escape sequence to arrive (terminal only - a pipe has
	   all its bytes immediately or is already at EOF). */
	{
		fd_set fds;
		struct timeval tv;

		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);
		tv.tv_sec = 0;
		tv.tv_usec = CPNBI_ESCAPE_TIMEOUT_USEC;

		return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv)
		       > 0;
	}
}

int32_t
cpnbi_get_event() {
	return cpnbi__decode_event(
	    cpnbi__getch, cpnbi__escape_followup_available, 0);
}

int32_t
cpnbi_get_unicode() {
	return cpnbi__decode_event(
	    cpnbi__getch, cpnbi__escape_followup_available, 1);
}

#endif

/* Decoded event: escape sequence decoder with optional
   UTF-8 support.  Returns packed int32_t per the CPNBI
   bit layout.

   Bytes are read via next_byte() (blocking).  After
   reading an ESC (0x1B), more_available() is called with
   a short timeout to distinguish lone Esc from the start
   of a multi-byte escape sequence.

   When utf8 is non-zero, non-ESC bytes in the multi-byte
   range (0xC0-0xFD) are decoded as UTF-8 and returned as
   Unicode code points.  When utf8 is zero, those bytes
   are returned individually as raw values (0-255).

   No timeout on UTF-8 continuation bytes — they arrive
   atomically with the start byte on any well-behaved
   terminal. */
int32_t
cpnbi__decode_event(int (*next_byte)(void),
                    int (*more_available)(void), int utf8) {
	int e, key = CPNBI_KEY_NUL, mod = CPNBI_MOD_NONE;
	int b2, b3, b4;

	/* Read one byte, propagating end-of-stream. A negative
	   value returned by next_byte() is CPNBI_EOF. */
#define CPNBI_NB(x)                                        \
	do {                                                     \
		(x) = next_byte();                                     \
		if ((x) < 0)                                           \
			return CPNBI_EOF;                                    \
	} while (0)

/* Timed variant used for the continuation bytes of an escape
   sequence. Before reading, it probes with more_available()
   (the same 25 ms timeout used to disambiguate a lone Esc).
   If nothing more arrives it aborts the sequence: CPNBI_EOF
   when the stream truly ended, otherwise CPNBI_KEY_ESCAPE
   (treated as a lone Escape). This is what prevents an
   incomplete sequence from blocking forever. */
#define CPNBI_NB_T(x)                                       \
	do {                                                     \
		if (!more_available())                              \
			return cpnbi__eof ? CPNBI_EOF                   \
			                  : CPNBI_KEY_ESCAPE;           \
		(x) = next_byte();                                  \
		if ((x) < 0)                                        \
			return CPNBI_EOF;                               \
	} while (0)

	CPNBI_NB(e);

	if (e == 27 && more_available()) {
		CPNBI_NB_T(e);
		switch (e) {
			case 'O':
				CPNBI_NB_T(e);
				switch (e) {
					case 'P': key = CPNBI_KEY_F1; break;
					case 'Q': key = CPNBI_KEY_F2; break;
					case 'R': key = CPNBI_KEY_F3; break;
					case 'S': key = CPNBI_KEY_F4; break;
				}
				break;
			case '[':
				CPNBI_NB_T(e);
				switch (e) {
					case 'A': key = CPNBI_KEY_UP; break;
					case 'B': key = CPNBI_KEY_DOWN; break;
					case 'C': key = CPNBI_KEY_RIGHT; break;
					case 'D': key = CPNBI_KEY_LEFT; break;
					case 'H': key = CPNBI_KEY_HOME; break;
					case 'F': key = CPNBI_KEY_END; break;
				case '[':
					CPNBI_NB_T(e);
					switch (e) {
							case 'A': key = CPNBI_KEY_F1; break;
							case 'B': key = CPNBI_KEY_F2; break;
							case 'C': key = CPNBI_KEY_F3; break;
							case 'D': key = CPNBI_KEY_F4; break;
							case 'E': key = CPNBI_KEY_F5; break;
						}
						break;
					default:
						switch (e) {
							case '1':
							case '2':
							case '3':
							case '4':
							case '5':
							case '6':
							case '7':
							case '8':
							case '9': {
								int num = e - '0';

							while (1) {
								CPNBI_NB_T(e);
								if (e < '0' || e > '9') {
										break;
									}
									num = num * 10 + (e - '0');
								}

								if (e == 'z') {
									switch (num) {
										case 224: key = CPNBI_KEY_F1; break;
										case 225: key = CPNBI_KEY_F2; break;
										case 226: key = CPNBI_KEY_F3; break;
										case 227: key = CPNBI_KEY_F4; break;
										case 228: key = CPNBI_KEY_F5; break;
										case 229: key = CPNBI_KEY_F6; break;
										case 230: key = CPNBI_KEY_F7; break;
										case 231: key = CPNBI_KEY_F8; break;
										case 232: key = CPNBI_KEY_F9; break;
										case 233: key = CPNBI_KEY_F10; break;
										case 192: key = CPNBI_KEY_F11; break;
										case 193: key = CPNBI_KEY_F12; break;
									}
								} else if (e == '~') {
									switch (num) {
										case 1: key = CPNBI_KEY_HOME; break;
										case 2: key = CPNBI_KEY_INSERT; break;
										case 3: key = CPNBI_KEY_DELETE; break;
										case 4: key = CPNBI_KEY_END; break;
										case 5: key = CPNBI_KEY_PAGE_UP; break;
										case 6:
											key = CPNBI_KEY_PAGE_DOWN;
											break;
										case 7: key = CPNBI_KEY_HOME; break;
										case 8: key = CPNBI_KEY_END; break;
										case 11: key = CPNBI_KEY_F1; break;
										case 12: key = CPNBI_KEY_F2; break;
										case 13: key = CPNBI_KEY_F3; break;
										case 14: key = CPNBI_KEY_F4; break;
										case 15: key = CPNBI_KEY_F5; break;
										case 17: key = CPNBI_KEY_F6; break;
										case 18: key = CPNBI_KEY_F7; break;
										case 19: key = CPNBI_KEY_F8; break;
										case 20: key = CPNBI_KEY_F9; break;
										case 21: key = CPNBI_KEY_F10; break;
										case 23: key = CPNBI_KEY_F11; break;
										case 24: key = CPNBI_KEY_F12; break;
									}
								} else if (e == ';') {
									switch (num) {
										case 1: key = CPNBI_KEY_HOME; break;
										case 2: key = CPNBI_KEY_INSERT; break;
										case 3: key = CPNBI_KEY_DELETE; break;
										case 4: key = CPNBI_KEY_END; break;
										case 5: key = CPNBI_KEY_PAGE_UP; break;
										case 6:
											key = CPNBI_KEY_PAGE_DOWN;
											break;
										case 7: key = CPNBI_KEY_HOME; break;
										case 8: key = CPNBI_KEY_END; break;
										case 11: key = CPNBI_KEY_F1; break;
										case 12: key = CPNBI_KEY_F2; break;
										case 13: key = CPNBI_KEY_F3; break;
										case 14: key = CPNBI_KEY_F4; break;
										case 15: key = CPNBI_KEY_F5; break;
										case 17: key = CPNBI_KEY_F6; break;
										case 18: key = CPNBI_KEY_F7; break;
										case 19: key = CPNBI_KEY_F8; break;
										case 20: key = CPNBI_KEY_F9; break;
										case 21: key = CPNBI_KEY_F10; break;
										case 23: key = CPNBI_KEY_F11; break;
										case 24: key = CPNBI_KEY_F12; break;
									}

									CPNBI_NB_T(mod);
									switch (mod) {
										case '2': mod = CPNBI_MOD_SHIFT; break;
										case '3': mod = CPNBI_MOD_ALT; break;
										case '4':
											mod = CPNBI_MOD_SHIFT | CPNBI_MOD_ALT;
											break;
										case '5': mod = CPNBI_MOD_CTRL; break;
										case '6':
											mod =
											    CPNBI_MOD_SHIFT | CPNBI_MOD_CTRL;
											break;
										case '7':
											mod = CPNBI_MOD_ALT | CPNBI_MOD_CTRL;
											break;
										case '8':
											mod = CPNBI_MOD_SHIFT | CPNBI_MOD_ALT
											      | CPNBI_MOD_CTRL;
											break;
									}

									CPNBI_NB_T(e);
									switch (e) {
										case 'A': key = CPNBI_KEY_UP; break;
										case 'B': key = CPNBI_KEY_DOWN; break;
										case 'C': key = CPNBI_KEY_RIGHT; break;
										case 'D': key = CPNBI_KEY_LEFT; break;
										case 'H': key = CPNBI_KEY_HOME; break;
										case 'F': key = CPNBI_KEY_END; break;
									}
								}
								break;
							}
						}
						break;
				}
				break;
		}
	} else if (e == 27) {
		key = CPNBI_KEY_ESCAPE;
	} else if (utf8 && (e & 0xE0) == 0xC0) {
		CPNBI_NB(b2);
		key = ((e & 0x1F) << 6) | (b2 & 0x3F);
	} else if (utf8 && (e & 0xF0) == 0xE0) {
		CPNBI_NB(b2);
		CPNBI_NB(b3);
		key = ((e & 0x0F) << 12) | ((b2 & 0x3F) << 6)
		      | (b3 & 0x3F);
	} else if (utf8 && (e & 0xF8) == 0xF0) {
		CPNBI_NB(b2);
		CPNBI_NB(b3);
		CPNBI_NB(b4);
		key = ((e & 0x07) << 18) | ((b2 & 0x3F) << 12)
		      | ((b3 & 0x3F) << 6) | (b4 & 0x3F);
	} else {
		key = e;
	}

#undef CPNBI_NB

	return key | (mod << CPNBI_MOD_OFFSET);
}


int32_t
cpnbi_event_key(int32_t event) {
	return event & CPNBI_VALUE_MASK;
}

int32_t
cpnbi_event_mod(int32_t event) {
	return (event >> CPNBI_MOD_OFFSET) & CPNBI_MOD_MASK;
}

int32_t
cpnbi_event_type(int32_t event) {
	return (event >> CPNBI_TYPE_OFFSET) & CPNBI_TYPE_MASK;
}

int32_t
cpnbi_event_is_special(int32_t event) {
	return (event & CPNBI_VALUE_MASK) > 0x10FFFF;
}