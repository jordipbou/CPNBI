/* tests/test_pipe_io_win.c
 *
 * Windows counterpart of tests/test_pipe_io.c: verifies CPNBI's
 * contract for non-TTY (piped) input on Windows by feeding
 * bytes through an anonymous pipe redirected onto the process
 * standard input handle and driving the same public API:
 *   - cpnbi_init() does NOT put a pipe into raw mode
 *   - get_char / get_event / get_unicode stream bytes and
 *     decode sequences/UTF-8 transparently
 *   - at end of stream they return CPNBI_EOF
 *   - is_char_available / is_event_available return 1 when a
 *     read will not block, including at EOF, and 0 only when
 *     a read would actually block
 *
 * Uses a real pipe (not a console); runs headless and CI-safe.
 */
#include <windows.h>

#include "cpnbi.h"
#include "unity.h"

/* Internal symbol, not part of the public header. */
void cpnbi__shutdown(void);

static HANDLE saved_stdin = INVALID_HANDLE_VALUE;
static HANDLE pipe_read = INVALID_HANDLE_VALUE;
static HANDLE pipe_write = INVALID_HANDLE_VALUE;

void
setUp(void) {}

void
tearDown(void) {}

/* Redirects the process standard input handle onto a fresh
   pipe, writes `len` bytes into the write end (then closes it
   so the read sees EOF), and calls cpnbi_init(). */
static void
pipe_setup(const unsigned char* bytes, size_t len) {
	BOOL ok;
	DWORD written = 0;

	ok = CreatePipe(&pipe_read, &pipe_write, NULL, 0);
	TEST_ASSERT_TRUE(ok);

	if (len > 0) {
		ok = WriteFile(pipe_write, bytes, (DWORD)len,
		              &written, NULL);
		TEST_ASSERT_TRUE(ok);
		TEST_ASSERT_EQUAL_UINT((unsigned int)len,
		                     (unsigned int)written);
	}
	CloseHandle(pipe_write);
	pipe_write = INVALID_HANDLE_VALUE;

	saved_stdin = GetStdHandle(STD_INPUT_HANDLE);
	SetStdHandle(STD_INPUT_HANDLE, pipe_read);

	cpnbi_init();
}

static void
pipe_teardown(void) {
	cpnbi__shutdown();

	if (saved_stdin != INVALID_HANDLE_VALUE) {
		SetStdHandle(STD_INPUT_HANDLE, saved_stdin);
		saved_stdin = INVALID_HANDLE_VALUE;
	}

	if (pipe_read != INVALID_HANDLE_VALUE) {
		CloseHandle(pipe_read);
		pipe_read = INVALID_HANDLE_VALUE;
	}
}

/* Keeps the write end open and stashed so the test can push
   bytes into it mid-run (to exercise blocking-vs-ready). */
static void
pipe_setup_open(void) {
	BOOL ok;

	ok = CreatePipe(&pipe_read, &pipe_write, NULL, 0);
	TEST_ASSERT_TRUE(ok);

	saved_stdin = GetStdHandle(STD_INPUT_HANDLE);
	SetStdHandle(STD_INPUT_HANDLE, pipe_read);

	cpnbi_init();
}

static void
pipe_teardown_open(void) {
	cpnbi__shutdown();

	if (pipe_write != INVALID_HANDLE_VALUE) {
		CloseHandle(pipe_write);
		pipe_write = INVALID_HANDLE_VALUE;
	}

	if (saved_stdin != INVALID_HANDLE_VALUE) {
		SetStdHandle(STD_INPUT_HANDLE, saved_stdin);
		saved_stdin = INVALID_HANDLE_VALUE;
	}

	if (pipe_read != INVALID_HANDLE_VALUE) {
		CloseHandle(pipe_read);
		pipe_read = INVALID_HANDLE_VALUE;
	}
}

void
test_pipe_char_stream_then_eof(void) {
	unsigned char in[] = {'a', 'b', 'c'};

	pipe_setup(in, sizeof(in));

	TEST_ASSERT_EQUAL_INT(1, cpnbi_is_char_available());
	TEST_ASSERT_EQUAL_INT('a', cpnbi_get_char());
	TEST_ASSERT_EQUAL_INT('b', cpnbi_get_char());
	TEST_ASSERT_EQUAL_INT('c', cpnbi_get_char());

	/* End of stream: a read won't block, so availability is 1
	   and the next read yields CPNBI_EOF. */
	TEST_ASSERT_EQUAL_INT(1, cpnbi_is_char_available());
	TEST_ASSERT_EQUAL_INT(CPNBI_EOF, cpnbi_get_char());

	/* Idempotent at EOF - the caller is expected to break. */
	TEST_ASSERT_EQUAL_INT(1, cpnbi_is_char_available());
	TEST_ASSERT_EQUAL_INT(CPNBI_EOF, cpnbi_get_char());

	pipe_teardown();
}

void
test_pipe_event_decodes_sequence_then_eof(void) {
	unsigned char in[] = {27, '[',
	                      'A'}; /* ESC [ A = Up arrow */

	pipe_setup(in, sizeof(in));

	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_UP, cpnbi_get_event());
	TEST_ASSERT_EQUAL_INT(1, cpnbi_is_event_available());
	TEST_ASSERT_EQUAL_INT(CPNBI_EOF, cpnbi_get_event());

	pipe_teardown();
}

void
test_pipe_unicode_decodes_then_eof(void) {
	unsigned char in[] = {0xE4, 0xBD, 0xA0}; /* U+4F60 */

	pipe_setup(in, sizeof(in));

	TEST_ASSERT_EQUAL_INT(0x4F60, cpnbi_get_unicode());
	TEST_ASSERT_EQUAL_INT(CPNBI_EOF, cpnbi_get_unicode());

	pipe_teardown();
}

void
test_pipe_lone_escape_at_eof(void) {
	unsigned char in[] = {27};

	pipe_setup(in, sizeof(in));

	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_ESCAPE,
	                      cpnbi_get_event());
	TEST_ASSERT_EQUAL_INT(CPNBI_EOF, cpnbi_get_event());

	pipe_teardown();
}

void
test_pipe_available_reports_blocking_vs_ready(void) {
	unsigned char byte = 'x';
	DWORD written = 0;

	pipe_setup_open();

	/* Write end open, no data yet: a read would block. */
	TEST_ASSERT_EQUAL_INT(0, cpnbi_is_char_available());

	TEST_ASSERT_TRUE(WriteFile(pipe_write, &byte, 1, &written,
	                          NULL));

	TEST_ASSERT_EQUAL_INT(1, cpnbi_is_char_available());
	TEST_ASSERT_EQUAL_INT('x', cpnbi_get_char());

	/* Close the write end: next read sees EOF without blocking. */
	CloseHandle(pipe_write);
	pipe_write = INVALID_HANDLE_VALUE;

	TEST_ASSERT_EQUAL_INT(1, cpnbi_is_char_available());
	TEST_ASSERT_EQUAL_INT(CPNBI_EOF, cpnbi_get_char());

	pipe_teardown_open();
}

int
main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_pipe_char_stream_then_eof);
	RUN_TEST(test_pipe_event_decodes_sequence_then_eof);
	RUN_TEST(test_pipe_unicode_decodes_then_eof);
	RUN_TEST(test_pipe_lone_escape_at_eof);
	RUN_TEST(test_pipe_available_reports_blocking_vs_ready);
	return UNITY_END();
}
