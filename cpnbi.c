#include <stdio.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <conio.h>
#include <windows.h>
#include "cpnbi.h"

static HANDLE hStdin;
static DWORD orig_mode;

void
cpnbi__shutdown() {
	SetConsoleMode(hStdin, orig_mode);
}

BOOL WINAPI
cpnbi__ctrl_handler(DWORD ctrl_type) {
	switch (ctrl_type) {
		case CTRL_C_EVENT: /* roughly equivalent to SIGINT */
		case CTRL_BREAK_EVENT:
		case CTRL_CLOSE_EVENT: /* console window closed - 
														 no POSIX equivalent */
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT: cpnbi__shutdown(); break;
	}
	return FALSE; /* let the default handler 
									 still run afterward */
}

void
cpnbi_init() {
	DWORD mode = 0;

	hStdin = GetStdHandle(STD_INPUT_HANDLE);
	GetConsoleMode(hStdin, &orig_mode);
	mode &=
	    ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT); // optional
	mode |= ENABLE_WINDOW_INPUT | ENABLE_PROCESSED_INPUT;
	SetConsoleMode(hStdin, mode);

	atexit(cpnbi__shutdown);
	SetConsoleCtrlHandler(cpnbi__ctrl_handler, TRUE);
}

int
cpnbi__process_event(INPUT_RECORD* record) {
	int key = CPNBI_KEY_NUL, mod = CPNBI_MOD_NONE;

	if (record->EventType == KEY_EVENT
	    && record->Event.KeyEvent.bKeyDown) {
		WORD vk = record->Event.KeyEvent.wVirtualKeyCode;
		DWORD ctrl = record->Event.KeyEvent.dwControlKeyState;

		if (ctrl & SHIFT_PRESSED) {
			mod |= CPNBI_MOD_SHIFT;
		}
		if (ctrl & LEFT_CTRL_PRESSED
		    || ctrl & RIGHT_CTRL_PRESSED) {
			mod |= CPNBI_MOD_CTRL;
		}
		if (ctrl & LEFT_ALT_PRESSED
		    || ctrl & RIGHT_ALT_PRESSED) {
			mod |= CPNBI_MOD_ALT;
		}

		switch (vk) {
			case VK_ESCAPE: key = CPNBI_KEY_ESCAPE; break;
			case VK_RETURN: key = CPNBI_KEY_ENTER; break;
			case VK_BACK: key = CPNBI_KEY_BACKSPACE; break;
			case VK_TAB: key = CPNBI_KEY_TAB; break;
			case VK_UP: key = CPNBI_KEY_UP; break;
			case VK_DOWN: key = CPNBI_KEY_DOWN; break;
			case VK_LEFT: key = CPNBI_KEY_LEFT; break;
			case VK_RIGHT: key = CPNBI_KEY_RIGHT; break;
			case VK_HOME: key = CPNBI_KEY_HOME; break;
			case VK_END: key = CPNBI_KEY_END; break;
			case VK_INSERT: key = CPNBI_KEY_INSERT; break;
			case VK_DELETE: key = CPNBI_KEY_DELETE; break;
			case VK_PRIOR: key = CPNBI_KEY_PAGE_UP; break;
			case VK_NEXT: key = CPNBI_KEY_PAGE_DOWN; break;
			case VK_F1: key = CPNBI_KEY_F1; break;
			case VK_F2: key = CPNBI_KEY_F2; break;
			case VK_F3: key = CPNBI_KEY_F3; break;
			case VK_F4: key = CPNBI_KEY_F4; break;
			case VK_F5: key = CPNBI_KEY_F5; break;
			case VK_F6: key = CPNBI_KEY_F6; break;
			case VK_F7: key = CPNBI_KEY_F7; break;
			case VK_F8: key = CPNBI_KEY_F8; break;
			case VK_F9: key = CPNBI_KEY_F9; break;
			case VK_F10: key = CPNBI_KEY_F10; break;
			case VK_F11: key = CPNBI_KEY_F11; break;
			case VK_F12: key = CPNBI_KEY_F12; break;
			default:
				int ch = record->Event.KeyEvent.uChar.AsciiChar;
				if (ch >= 32 && ch <= 126) {
					key = ch;
				}
		}
	}

	return key | (mod << 16);
}

int
cpnbi_is_char_available() {
	DWORD count;
	INPUT_RECORD record;

	PeekConsoleInput(hStdin, &record, 1, &count);

	if (count > 0) {
		int res = cpnbi__process_event(&record);
		int key = cpnbi_event_key(res);

		if (key >= 32 && key <= 126) {
			return 1;
		} else {
			/* Consume non useful event */
			ReadConsoleInput(hStdin, &record, 1, &count);
		}
	}

	return 0;
}

int
cpnbi_is_event_available() {
	DWORD count;
	INPUT_RECORD record;

	PeekConsoleInput(hStdin, &record, 1, &count);

	if (count > 0) {
		int res = cpnbi__process_event(&record);

		if (cpnbi_event_key(res) != 0) {
			return 1;
		} else {
			/* Consume non useful event */
			ReadConsoleInput(hStdin, &record, 1, &count);
		}
	}

	return 0;
}

int
cpnbi_get_char() {
	DWORD count;
	INPUT_RECORD record;

	while (1) {
		ReadConsoleInput(hStdin, &record, 1, &count);

		if (count > 0) {
			int res = cpnbi__process_event(&record);

			if (cpnbi_event_key(res) >= 32
			    && cpnbi_event_key(res) <= 126) {
				return cpnbi_event_key(res);
			}
		}
	}
}

int
cpnbi_get_event() {
	DWORD count;
	INPUT_RECORD record;

	while (1) {
		ReadConsoleInput(hStdin, &record, 1, &count);

		if (count > 0) {
			int res = cpnbi__process_event(&record);

			if (cpnbi_event_key(res) != 0) {
				return res;
			}
		}
	}
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

void
cpnbi__shutdown() {
	tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

static void
cpnbi__signal_handler(int signum) {
	cpnbi__shutdown();
	/* Reset to default handling and re-raise, rather 
		 than exit()/_exit() directly, so the process still 
		 terminates via the signal itself - this preserves 
		 normal Unix semantics (e.g. the shell reporting
	   "Terminated" and $? reflecting death-by-signal), 
		 rather than masquerading as a clean exit. */
	signal(signum, SIG_DFL);
	raise(signum);
}

void
cpnbi_init() {
	struct termios raw;

	tcgetattr(STDIN_FILENO, &orig_termios);
	raw = orig_termios;
	raw.c_lflag &= ~(ICANON | ECHO);
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSANOW, &raw);

	/* Not calling cpnbi__shutdown when exiting the host
		 program means the terminal will stay in an incorrect
		 mode. That happens if using exit() or on SIGINT
		 and SIGTERM. 
		 Here, cpnbi__shutdown and cpnbi__signal_handler are
		 called on each required case to leave the terminal
		 in a correct state.
		 */
	atexit(cpnbi__shutdown);
	signal(SIGINT, cpnbi__signal_handler);
	signal(SIGTERM, cpnbi__signal_handler);
}

int
cpnbi__getch() {
	return getchar();
}

int
cpnbi_is_char_available(void) {
	int ch;
	int oldf;

	oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
	ch = cpnbi__getch();
	fcntl(STDIN_FILENO, F_SETFL, oldf);

	if (ch == EOF) {
		return 0;
	}

	ungetc(ch, stdin);
	return (ch >= 32 && ch <= 126);
}

int
cpnbi_is_event_available(void) {
	int ch;
	int oldf;

	oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

	ch = cpnbi__getch();

	fcntl(STDIN_FILENO, F_SETFL, oldf);

	if (ch != EOF) {
		ungetc(ch, stdin);
		return 1;
	}

	return 0;
}

/* Forward declarations: decode_event and */
/* escape_followup_available are defined later. */
int cpnbi__decode_event(int (*next_byte)(void),
                        int (*more_available)(void));
static int cpnbi__escape_followup_available(void);

int
cpnbi_get_char() {
	while (1) {
		int event = cpnbi__decode_event(
		    cpnbi__getch, cpnbi__escape_followup_available);
		int key = cpnbi_event_key(event);

		if (key >= 32 && key <= 126) {
			return key;
		}
	}
}

/* cpnbi__decode_event: pure escape-sequence decoder.     */

/* Takes a "give me the next byte" function instead of    */
/* reading from stdin directly - this is what makes the   */
/* decode logic unit-testable without a real terminal:    */
/* production code passes cpnbi__getch, tests pass a mock */
/* that walks a fixed byte array.                         */
int
cpnbi__decode_event(int (*next_byte)(void),
                    int (*more_available)(void)) {
	int e, key = CPNBI_KEY_NUL, mod = CPNBI_MOD_NONE;

	if ((e = next_byte()) == 27 && more_available()) {
		/* ANSI escape sequence (Linux only) */
		switch (e = next_byte()) {
			case 'O':
				/* F1 to F4 without modifiers */
				switch (e = next_byte()) {
					case 'P': key = CPNBI_KEY_F1; break;
					case 'Q': key = CPNBI_KEY_F2; break;
					case 'R': key = CPNBI_KEY_F3; break;
					case 'S': key = CPNBI_KEY_F4; break;
				}
				break;
			case '[':
				/* ESC [ control sequence introducer */
				switch (e = next_byte()) {
					case 'A': key = CPNBI_KEY_UP; break;
					case 'B': key = CPNBI_KEY_DOWN; break;
					case 'C': key = CPNBI_KEY_RIGHT; break;
					case 'D': key = CPNBI_KEY_LEFT; break;
					case 'H': key = CPNBI_KEY_HOME; break;
					case 'F': key = CPNBI_KEY_END; break;
					case '[':
						/* Linux virtual console (VT) sends F1-F5 as
               ESC [ [ A .. ESC [ [ E -- this is NOT xterm's
               ESC O P style and needs its own branch. */
						switch (e = next_byte()) {
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
								/* Numeric CSI parameter: accumulate all
							   digits instead of assuming at most two,
							   then branch on the terminating byte.
							   This is needed because plain xterm-style
							   sequences ("CSI Pn ~") only ever use
							   1-2 digit parameters, but the Sun-style
							   function-key sequences some consoles
							   (e.g. certain VirtualBox text consoles)
							   send - "CSI Pn z" - use 3-digit
							   parameters such as 224 for F1. */
								int num = e - '0';

								while ((e = next_byte()) >= '0'
								       && e <= '9') {
									num = num * 10 + (e - '0');
								}

								if (e == 'z') {
									/* Sun-style function keys */
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
									/* Parameter;Modifier<final> form,
								   e.g. "1;5A" = Ctrl+Up,
								   "3;2~" = Shift+Delete. */
									switch (num) {
										case 1: key = CPNBI_KEY_HOME; break;
										case 2: key = CPNBI_KEY_INSERT; break;
										case 3: key = CPNBI_KEY_DELETE; break;
										case 4: key = CPNBI_KEY_END; break;
										case 5: key = CPNBI_KEY_PAGE_UP; break;
										case 6:
											key = CPNBI_KEY_PAGE_DOWN;
											break;
									}

									switch (mod = next_byte()) {
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

									/* Consume the final byte: '~' for
								   Home/Insert/Delete/End/PgUp/PgDn,
								   or a letter for arrow-like keys
								   sent as "1;<mod><letter>". */
									switch (e = next_byte()) {
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
		/* Esc pressed alone, nothing else waiting */
		key = CPNBI_KEY_ESCAPE;
	} else {
		key = e;
	}

	return key | (mod << 16);
}

/* Used only to disambiguate a lone Esc keypress from 
	 the start of an escape sequence (point 5). Unlike 
	 cpnbi_is_event_available(), this deliberately waits 
	 briefly instead of polling instantly - a real sequence 
	 arriving with some latency shouldn't be misread as a lone
   Esc. Not used for general-purpose polling; 
	 cpnbi_is_event_available() keeps its existing 
	 instant-check behavior for that. */
static int
cpnbi__escape_followup_available(void) {
	fd_set fds;
	struct timeval tv;

	/* select() only sees the kernel-level fd buffer, 
		 but next_byte() reads through buffered stdio 
		 (getchar()) - and any earlier buffered read 
		 (e.g. is_char_available()/is_event_available()) may 
		 have already pulled subsequent bytes out of the kernel 
		 and into stdio's user-space buffer, invisible to 
		 select(). Check that buffer first, the same way 
		 those functions do, before falling back to a 
		 real wait. */

	if (!cpnbi_is_event_available()) {
		/* Nothing buffered yet - wait up to the timeout for 
		   the rest of the sequence to actually arrive at the 
		   kernel */
		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);
		tv.tv_sec = 0;
		tv.tv_usec = CPNBI_ESCAPE_TIMEOUT_USEC;

		return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv)
		       > 0;
	}

	return 1;
}

int
cpnbi_get_event() {
	return cpnbi__decode_event(
	    cpnbi__getch, cpnbi__escape_followup_available);
}

#endif

int
cpnbi_event_key(int event) {
	return event & 0xFFFF;
}

int
cpnbi_event_mod(int event) {
	return (event >> 16) & 0xFF;
}
