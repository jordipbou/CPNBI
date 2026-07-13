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
#include <pthread.h>
#include <pty.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* Internal symbol, not part of the public header, */
/* declared here for testing purposes only. */
void cpnbi__shutdown();

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
	cpnbi__shutdown();
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

	TEST_ASSERT_EQUAL_INT(1, cpnbi_is_byte_available());
	TEST_ASSERT_EQUAL_INT('a', cpnbi_get_byte());
}

/* is_byte_available() returns 1 for ANY byte, */
/* including non-printable ones like ESC. */
void
test_raw_char_available_returns_any_byte(void) {
	unsigned char esc_byte[] = {27};
	type_bytes(esc_byte, sizeof(esc_byte));

	TEST_ASSERT_EQUAL_INT(1, cpnbi_is_byte_available());
	TEST_ASSERT_EQUAL_INT(27, cpnbi_get_byte());
}

/* Repeated is_byte_available() checks must not consume */
/* the byte.  The event reader must still decode correctly. */
void
test_repeated_char_available_checks_are_idempotent(void) {
	unsigned char up_arrow[] = {27, '[', 'A'};
	type_bytes(up_arrow, sizeof(up_arrow));

	TEST_ASSERT_EQUAL_INT(1, cpnbi_is_byte_available());
	TEST_ASSERT_EQUAL_INT(1, cpnbi_is_byte_available());

	/* get_event() should still decode the full sequence */
	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_UP, cpnbi_get_event());
}

/* get_byte() returns raw bytes with no filtering */
void
test_get_byte_returns_raw_bytes_in_order(void) {
	unsigned char input[] = {0x01, 'a'};
	type_bytes(input, sizeof(input));

	TEST_ASSERT_EQUAL_INT(0x01, cpnbi_get_byte());
	TEST_ASSERT_EQUAL_INT('a', cpnbi_get_byte());
}

/* get_byte() returns raw escape sequence bytes — the */
/* caller is responsible for interpretation. */
void
test_get_byte_returns_raw_escape_bytes(void) {
	unsigned char input[] = {27, '[', 'A', 'b'};
	type_bytes(input, sizeof(input));

	TEST_ASSERT_EQUAL_INT(27,  cpnbi_get_byte());
	TEST_ASSERT_EQUAL_INT('[', cpnbi_get_byte());
	TEST_ASSERT_EQUAL_INT('A', cpnbi_get_byte());
	TEST_ASSERT_EQUAL_INT('b', cpnbi_get_byte());
}

struct delayed_write_args {
	int fd;
	const unsigned char* bytes;
	size_t len;
	useconds_t delay_usec;
};

static void*
delayed_write_thread(void* arg) {
	struct delayed_write_args* args =
	    (struct delayed_write_args*)arg;
	usleep(args->delay_usec);
	write(args->fd, args->bytes, args->len);
	return NULL;
}

/* Regression test: the rest of an escape sequence arriving
	 with a short delay (simulating SSH/tmux/slow-pty latency) 
	 must still be decoded correctly, 
	 not misread as a lone Esc. */
void
test_delayed_escape_sequence_still_decoded_correctly(void) {
	unsigned char esc_byte[] = {27};
	unsigned char rest[] = {'[', 'A'}; /* completes ESC [ A 
		                                    = Up arrow */
	pthread_t writer;
	struct delayed_write_args args = {
	    .fd = master_fd,
	    .bytes = rest,
	    .len = sizeof(rest),
	    .delay_usec = 10000 /* 10ms - comfortably 
				                     inside the 25ms window */
	};

	type_bytes(esc_byte, sizeof(esc_byte)); /* ESC arrives 
		                                         immediately */
	pthread_create(&writer, NULL, delayed_write_thread,
	               &args);

	/* cpnbi_get_event() blocks reading ESC (already 
		 available), then waits up to 25ms for the rest - 
		 which arrives at the 10ms mark. */
	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_UP, cpnbi_get_event());

	pthread_join(writer, NULL);
}

/* Sanity check the other direction: a genuinely lone Esc 
	 (nothing following, ever) must still resolve to 
	 CPNBI_KEY_ESCAPE, not hang for 25ms waiting for something 
	 that'll never come, and not misfire. */
void
test_lone_escape_still_resolves_correctly_with_timeout_in_place(
    void) {
	unsigned char esc_byte[] = {27};
	type_bytes(esc_byte, sizeof(esc_byte));

	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_ESCAPE,
	                      cpnbi_get_event());
}

/* A partial escape sequence (ESC [ with no terminator) must
   time out and resolve to a lone Esc rather than block
   forever. Exercises the real select()-based timeout path. */
void
test_partial_escape_via_pty_times_out(void) {
	unsigned char partial[] = {27, '['};

	type_bytes(partial, sizeof(partial));

	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_ESCAPE,
	                      cpnbi_get_event());
}

int
main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_plain_char_is_available_and_read_correctly);
	RUN_TEST(test_raw_char_available_returns_any_byte);
	RUN_TEST(test_repeated_char_available_checks_are_idempotent);
	RUN_TEST(test_get_byte_returns_raw_bytes_in_order);
	RUN_TEST(test_get_byte_returns_raw_escape_bytes);
	RUN_TEST(
	    test_delayed_escape_sequence_still_decoded_correctly);
	RUN_TEST(
	    test_lone_escape_still_resolves_correctly_with_timeout_in_place);
	RUN_TEST(test_partial_escape_via_pty_times_out);
	return UNITY_END();
}
