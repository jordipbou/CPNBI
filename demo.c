#include <locale.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define STDOUT_FILENO _fileno(stdout)
#else
#include <unistd.h>
#endif
#include <wchar.h>

#include "cpnbi.h"

static int use_color;

#define C_DIM (use_color ? "\033[2m" : "")
#define C_KEY (use_color ? "\033[1;36m" : "")
#define C_MOD (use_color ? "\033[1;33m" : "")
#define C_SPC (use_color ? "\033[1;35m" : "")
#define C_RST (use_color ? "\033[0m" : "")

/* Render a key value into buf. unicode_mode only affects how
   a non-special value is described (as a code point). */
static void
describe_key(int32_t key, int unicode_mode, char* buf,
             size_t n) {
	if (key >= CPNBI_KEY_BASE) {
		switch (key) {
			case CPNBI_KEY_UP: snprintf(buf, n, "Up"); return;
			case CPNBI_KEY_DOWN: snprintf(buf, n, "Down"); return;
			case CPNBI_KEY_LEFT: snprintf(buf, n, "Left"); return;
			case CPNBI_KEY_RIGHT:
				snprintf(buf, n, "Right");
				return;
			case CPNBI_KEY_HOME: snprintf(buf, n, "Home"); return;
			case CPNBI_KEY_END: snprintf(buf, n, "End"); return;
			case CPNBI_KEY_INSERT:
				snprintf(buf, n, "Insert");
				return;
			case CPNBI_KEY_DELETE:
				snprintf(buf, n, "Delete");
				return;
			case CPNBI_KEY_PAGE_UP:
				snprintf(buf, n, "PageUp");
				return;
			case CPNBI_KEY_PAGE_DOWN:
				snprintf(buf, n, "PageDown");
				return;
			case CPNBI_KEY_F1: snprintf(buf, n, "F1"); return;
			case CPNBI_KEY_F2: snprintf(buf, n, "F2"); return;
			case CPNBI_KEY_F3: snprintf(buf, n, "F3"); return;
			case CPNBI_KEY_F4: snprintf(buf, n, "F4"); return;
			case CPNBI_KEY_F5: snprintf(buf, n, "F5"); return;
			case CPNBI_KEY_F6: snprintf(buf, n, "F6"); return;
			case CPNBI_KEY_F7: snprintf(buf, n, "F7"); return;
			case CPNBI_KEY_F8: snprintf(buf, n, "F8"); return;
			case CPNBI_KEY_F9: snprintf(buf, n, "F9"); return;
			case CPNBI_KEY_F10: snprintf(buf, n, "F10"); return;
			case CPNBI_KEY_F11: snprintf(buf, n, "F11"); return;
			case CPNBI_KEY_F12: snprintf(buf, n, "F12"); return;
			case CPNBI_KEY_BACKSPACE:
				snprintf(buf, n, "Backspace");
				return;
			case CPNBI_KEY_TAB: snprintf(buf, n, "Tab"); return;
			case CPNBI_KEY_ENTER:
				snprintf(buf, n, "Enter");
				return;
			case CPNBI_KEY_ESCAPE:
				snprintf(buf, n, "Esc");
				return;
			default: snprintf(buf, n, "Special(%d)", key); return;
		}
	}

	if (key == CPNBI_KEY_NUL) {
		snprintf(buf, n, "NUL");
		return;
	}

	/* Named control keys (both modes) - handled before the
	   generic Ctrl+letter mapping so the Enter/Esc/Tab keys
	   show readable names rather than Ctrl+J / 0x1B. */
	switch (key) {
		case 8: snprintf(buf, n, "Backspace"); return;
		case 9: snprintf(buf, n, "Tab"); return;
		case 10:
		case 13: snprintf(buf, n, "Enter"); return;
		case 27: snprintf(buf, n, "Esc"); return;
		case 127: snprintf(buf, n, "DEL"); return;
	}

	if (key >= 1 && key <= 26) {
		snprintf(buf, n, "Ctrl+%c", 'A' + (key - 1));
		return;
	}

	if (key >= 32 && key <= 126) {
		snprintf(buf, n, "'%c'", (char)key);
		return;
	}

	if (unicode_mode && key >= 0x20 && key != 0x7F
	    && key <= 0x10FFFF) {
		int w = snprintf(buf, n, "%lc", (wint_t)key);

		if (w > 0 && (size_t)w < n) {
			snprintf(buf + w, n - (size_t)w, " (U+%04X)", key);
		} else {
			snprintf(buf, n, "U+%04X", key);
		}
		return;
	}

	snprintf(buf, n, "0x%02X", key);
}

/* Ordered modifier flag table for stable rendering. */
static const struct {
	int flag;
	const char* name;
} mod_table[] = {
    {CPNBI_MOD_SHIFT, "Shift"},
    {CPNBI_MOD_CTRL, "Ctrl"},
    {CPNBI_MOD_ALT, "Alt"},
    {CPNBI_MOD_SUPER, "Super"},
    {CPNBI_MOD_HYPER, "Hyper"},
    {CPNBI_MOD_META, "Meta"},
    {CPNBI_MOD_CAPS_LOCK, "CapsLock"},
    {CPNBI_MOD_NUM_LOCK, "NumLock"},
};

static void
describe_mods(int32_t mod, char* buf, size_t n) {
	size_t i;
	int any = 0;

	buf[0] = '\0';

	for (i = 0; i < sizeof(mod_table) / sizeof(mod_table[0]);
	     i++) {
		if (mod & mod_table[i].flag) {
			if (any) {
				strncat(buf, "+", n - strlen(buf) - 1);
			}
			strncat(buf, mod_table[i].name, n - strlen(buf) - 1);
			any = 1;
		}
	}

	if (!any) {
		snprintf(buf, n, "-");
	}
}

int
main(int argc, char** argv) {
	int unicode_mode = 0;
	int32_t (*read_fn)(void);
	int32_t e;
	char key_buf[64];
	char mod_buf[64];

	setlocale(LC_ALL, "");

	if (argc > 1
	    && (strcmp(argv[1], "--unicode") == 0
	        || strcmp(argv[1], "-u") == 0)) {
		unicode_mode = 1;
	}

	read_fn =
	    unicode_mode ? cpnbi_get_unicode : cpnbi_get_event;

	use_color = isatty(STDOUT_FILENO);

	cpnbi_init();

	fprintf(stderr,
	        "CPNBI demo — %s mode. "
	        "Ctrl+C quits; piped input ends at EOF.\n",
	        unicode_mode ? "unicode" : "event");

	while ((e = read_fn()) != CPNBI_EOF) {
		int32_t key = cpnbi_event_key(e);
		int32_t mod = cpnbi_event_mod(e);
		int special = cpnbi_event_is_special(e);

		if (key == 3 && !special) {
			fprintf(stderr, "Quit (Ctrl+C)\n");
			break;
		}

		describe_key(key, unicode_mode, key_buf,
		             sizeof(key_buf));
		describe_mods(mod, mod_buf, sizeof(mod_buf));

		printf("%sKEY%s  %s%-10s%s  %sMOD%s  %-16s  "
		       "%sTYPE%s press  %sSPECIAL%s %s%s\n",
		       C_DIM, C_RST, C_KEY, key_buf, C_RST, C_DIM,
		       C_RST, mod_buf, C_DIM, C_RST, C_DIM, C_RST,
		       special ? C_SPC : "", special ? "yes" : "no");
	}

	fprintf(stderr, "End of input — bye.\n");

	return 0;
}
