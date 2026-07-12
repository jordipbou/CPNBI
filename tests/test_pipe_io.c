/* tests/test_pipe_io.c
 *
 * Verifies CPNBI's contract for non-TTY (piped) input:
 *   - cpnbi_init() does NOT put a pipe into raw mode
 *   - get_char / get_event / get_unicode stream bytes and
 *     decode sequences/UTF-8 transparently
 *   - at end of stream they return CPNBI_EOF
 *   - is_char_available / is_event_available return 1 when a
 *     read will not block, including at EOF, and 0 only when
 *     a read would actually block
 *
 * Uses a real pipe (not a pty); runs headless and CI-safe.
 */
#include "cpnbi.h"
#include "unity.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

/* Internal symbol, not part of the public header, */
/* declared here for testing purposes only. */
void cpnbi__shutdown();

static int saved_stdin_fd = -1;
static int pipe_read_fd = -1;
static int pipe_write_fd = -1;

void
setUp(void) {}

void
tearDown(void) {}

static void
pipe_setup(const unsigned char* bytes, size_t len) {
	int fds[2];

	TEST_ASSERT_EQUAL_INT(0, pipe(fds));
	TEST_ASSERT_EQUAL_INT((int)len,
	                      (int)write(fds[1], bytes, len));
	close(
	    fds[1]); /* close write end: immediate EOF after read */

	pipe_read_fd = fds[0];
	saved_stdin_fd = dup(STDIN_FILENO);
	TEST_ASSERT_TRUE(saved_stdin_fd >= 0);
	TEST_ASSERT_EQUAL_INT(STDIN_FILENO,
	                      dup2(pipe_read_fd, STDIN_FILENO));

	/* The stdio stdin stream carries EOF/error flags and a
	   buffer across the fd swap; clear them and disable
	   buffering so each getchar maps to one read() syscall
	   (no readahead, no stale EOF from a previous test). */
	clearerr(stdin);
	setvbuf(stdin, NULL, _IONBF, 0);

	cpnbi_init();
}

static void
pipe_teardown(void) {
	cpnbi__shutdown();

	if (saved_stdin_fd >= 0) {
		dup2(saved_stdin_fd, STDIN_FILENO);
		close(saved_stdin_fd);
		saved_stdin_fd = -1;
	}

	if (pipe_read_fd >= 0) {
		close(pipe_read_fd);
		pipe_read_fd = -1;
	}
}

/* Reads a byte stream through a pipe whose write end is kept
   open, to exercise the "would block" vs "ready" paths. */
static void
pipe_setup_open(void) {
	int fds[2];

	TEST_ASSERT_EQUAL_INT(0, pipe(fds));

	pipe_read_fd = fds[0];
	saved_stdin_fd = dup(STDIN_FILENO);
	TEST_ASSERT_TRUE(saved_stdin_fd >= 0);
	TEST_ASSERT_EQUAL_INT(STDIN_FILENO,
	                      dup2(pipe_read_fd, STDIN_FILENO));

	clearerr(stdin);
	setvbuf(stdin, NULL, _IONBF, 0);

	/* Stash the write end so the test can push bytes. */
	pipe_write_fd = fds[1];

	cpnbi_init();
}

static void
pipe_teardown_open(void) {
	cpnbi__shutdown();
	close(pipe_write_fd);
	pipe_write_fd = -1;

	if (saved_stdin_fd >= 0) {
		dup2(saved_stdin_fd, STDIN_FILENO);
		close(saved_stdin_fd);
		saved_stdin_fd = -1;
	}

	if (pipe_read_fd >= 0) {
		close(pipe_read_fd);
		pipe_read_fd = -1;
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

	pipe_setup_open();

	/* Write end open, no data yet: a read would block. */
	TEST_ASSERT_EQUAL_INT(0, cpnbi_is_char_available());

	TEST_ASSERT_EQUAL_INT(
	    1, (int)write(pipe_write_fd, &byte, 1));

	TEST_ASSERT_EQUAL_INT(1, cpnbi_is_char_available());
	TEST_ASSERT_EQUAL_INT('x', cpnbi_get_char());

	/* Close the write end: next read sees EOF without blocking. */
	close(pipe_write_fd);
	pipe_write_fd = -1;

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
