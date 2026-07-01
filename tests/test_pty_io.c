/* tests/test_pty_io.c
 *
 * I/O-level regression tests for the Linux byte-reading 
 * layer.
 * Uses a real pty so termios/fcntl/ungetc all behave 
 * exactly as they would against a genuine terminal, 
 * but fully scriptable from the test.
 */
#include "cpnbi.h"
#include "unity.h"

#include <fcntl.h>
#include <pty.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static int master_fd;
static int slave_fd;
static int saved_stdin_fd;

void
setUp(void) {
	struct termios raw;

	TEST_ASSERT_EQUAL_INT_MESSAGE(
	    0, openpty(&master_fd, &slave_fd, NULL, NULL, NULL),
	    "openpty() failed - is a pty available in this "
	    "environment?");

	/* Force raw mode up front so bytes are visible */
	/* immediately instead of	being queued by the line */
	/* discipline until a newline arrives. */
	TEST_ASSERT_EQUAL_INT(0, tcgetattr(slave_fd, &raw));
	cfmakeraw(&raw);
	TEST_ASSERT_EQUAL_INT(0,
	                      tcsetattr(slave_fd, TCSANOW, &raw));

	/* cpnbi always reads STDIN_FILENO / stdin - redirect it to our fake tty. */
	saved_stdin_fd = dup(STDIN_FILENO);
	TEST_ASSERT_TRUE_MESSAGE(saved_stdin_fd >= 0,
	                         "failed to save real stdin");
	TEST_ASSERT_EQUAL_INT_MESSAGE(
	    STDIN_FILENO, dup2(slave_fd, STDIN_FILENO),
	    "failed to redirect stdin to pty slave");

	cpnbi_init();
}

void
tearDown(void) {
	cpnbi_shutdown();
	dup2(saved_stdin_fd, STDIN_FILENO);
	close(saved_stdin_fd);
	close(master_fd);
	close(slave_fd);
}

/* Simulates "typing" bytes into the fake terminal. */
static void
type_bytes(const unsigned char* bytes, size_t len) {
	ssize_t written = write(master_fd, bytes, len);
	TEST_ASSERT_EQUAL_INT_MESSAGE(
	    (int)len, (int)written,
	    "failed to write all bytes to pty master");
}

/* --- Tests --- */

void
test_plain_char_is_available_and_read_correctly(void) {
	unsigned char input[] = {'a'};
	type_bytes(input, sizeof(input));

	TEST_ASSERT_EQUAL_INT(1, cpnbi_is_char_available());
	TEST_ASSERT_EQUAL_INT('a', cpnbi_get_char());
}

/* Regression test for the bug: is_char_available() must */
/* not consume a byte it decides is "not a plain char" */
/* - e.g. the ESC starting an arrow key. */
void
test_escape_sequence_byte_is_not_lost_by_char_available_check(
    void) {
	unsigned char up_arrow[] = {27, '[',
	                            'A'}; /* ESC [ A = Up arrow */
	type_bytes(up_arrow, sizeof(up_arrow));

	/* ESC isn't printable, so this must correctly say */
	/* "no plain char"... */
	TEST_ASSERT_EQUAL_INT(0, cpnbi_is_char_available());

	/* ...but the ESC byte must still be there for the event */
	/* reader. */
	TEST_ASSERT_EQUAL_INT(1, cpnbi_is_event_available());
	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_UP, cpnbi_get_event());
}

void
test_repeated_char_available_checks_are_idempotent(void) {
	unsigned char up_arrow[] = {27, '[', 'A'};
	type_bytes(up_arrow, sizeof(up_arrow));

	/* A buggy implementation that drops the first byte */
	/* will report differently on the second call, since */
	/* it's now looking at '[' (91), which IS a printable */
	/* char - exposing the bug directly. */
	TEST_ASSERT_EQUAL_INT(0, cpnbi_is_char_available());
	TEST_ASSERT_EQUAL_INT(0, cpnbi_is_char_available());

	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_UP, cpnbi_get_event());
}

int
main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_plain_char_is_available_and_read_correctly);
	RUN_TEST(
	    test_escape_sequence_byte_is_not_lost_by_char_available_check);
	RUN_TEST(
	    test_repeated_char_available_checks_are_idempotent);
	return UNITY_END();
}
