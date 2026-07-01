/* tests/test_termios_lifecycle.c
 *
 * Verifies that cpnbi_init()/cpnbi__shutdown() correctly 
 * own the raw-mode lifecycle (point 3): init() should 
 * disable ICANON/ECHO exactly once, and shutdown() should 
 * restore the terminal to exactly what it was before 
 * init() touched it.
 *
 * Uses a real pty so termios behaves as it would against 
 * a genuine terminal. Unlike test_pty_io.c, this fixture 
 * does NOT pre-force raw mode - we want to observe 
 * cpnbi_init()'s own effect on a pristine pty, and since 
 * no actual data is written/read here, there's no risk of 
 * the canonical-mode line-discipline hang that motivated 
 * forcing raw mode in the I/O tests.
 */
#include "cpnbi.h"
#include "unity.h"

#include <pty.h>
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
	TEST_ASSERT_EQUAL_INT_MESSAGE(
	    0, openpty(&master_fd, &slave_fd, NULL, NULL, NULL),
	    "openpty() failed - is a pty available in this "
	    "environment?");

	saved_stdin_fd = dup(STDIN_FILENO);
	TEST_ASSERT_TRUE_MESSAGE(saved_stdin_fd >= 0,
	                         "failed to save real stdin");
	TEST_ASSERT_EQUAL_INT_MESSAGE(
	    STDIN_FILENO, dup2(slave_fd, STDIN_FILENO),
	    "failed to redirect stdin to pty slave");

	/* Deliberately NOT forcing raw mode here - we want 
		 cpnbi_init() to be the thing that changes the 
		 terminal's mode, so we can verify it actually does. */
}

void
tearDown(void) {
	/* Note: does NOT call cpnbi__shutdown() here - each 
		 test controls the init/shutdown calls itself, since 
		 that's exactly what's under test. */
	dup2(saved_stdin_fd, STDIN_FILENO);
	close(saved_stdin_fd);
	close(master_fd);
	close(slave_fd);
}

/* --- Tests --- */

void
test_init_disables_icanon_and_echo(void) {
	struct termios before, during;

	TEST_ASSERT_EQUAL_INT(0,
	                      tcgetattr(STDIN_FILENO, &before));

	/* Sanity check: a fresh pty slave should start in 
		 canonical mode with echo on, same as a real 
		 terminal - if this fails, the test environment itself 
		 is unusual and the rest of the test isn't meaningful. */
	TEST_ASSERT_TRUE_MESSAGE(
	    before.c_lflag & ICANON,
	    "expected fresh pty to start with ICANON set");
	TEST_ASSERT_TRUE_MESSAGE(
	    before.c_lflag & ECHO,
	    "expected fresh pty to start with ECHO set");

	cpnbi_init();

	TEST_ASSERT_EQUAL_INT(0,
	                      tcgetattr(STDIN_FILENO, &during));
	TEST_ASSERT_FALSE_MESSAGE(
	    during.c_lflag & ICANON,
	    "cpnbi_init() should have disabled ICANON");
	TEST_ASSERT_FALSE_MESSAGE(
	    during.c_lflag & ECHO,
	    "cpnbi_init() should have disabled ECHO");

	cpnbi__shutdown(); /* leave the fd in a clean state 
											 before tearDown */
}

void
test_shutdown_restores_original_settings(void) {
	struct termios before, after;

	TEST_ASSERT_EQUAL_INT(0,
	                      tcgetattr(STDIN_FILENO, &before));

	cpnbi_init();
	cpnbi__shutdown();

	TEST_ASSERT_EQUAL_INT(0, tcgetattr(STDIN_FILENO, &after));

	/* Compare the full lflag, not just ICANON/ECHO - 
		 shutdown() should put back exactly what was there, 
		 not just flip the two bits we know init() touches. 
		 Catches any accidental extra state change. */
	TEST_ASSERT_EQUAL_UINT(before.c_lflag, after.c_lflag);
	TEST_ASSERT_EQUAL_UINT(before.c_iflag, after.c_iflag);
	TEST_ASSERT_EQUAL_UINT(before.c_oflag, after.c_oflag);
	TEST_ASSERT_EQUAL_UINT(before.c_cflag, after.c_cflag);
}

void
test_init_is_idempotent_with_shutdown_in_between(void) {
	/* Guards against orig_termios getting clobbered by a 
		 second init() call after a shutdown() - e.g. if 
		 init() were ever accidentally called twice without 
		 an intervening shutdown, or vice versa, we still 
		 want a single init/shutdown pair to round-trip 
		 cleanly. */
	struct termios before, after;

	TEST_ASSERT_EQUAL_INT(0,
	                      tcgetattr(STDIN_FILENO, &before));

	cpnbi_init();
	cpnbi__shutdown();
	cpnbi_init();
	cpnbi__shutdown();

	TEST_ASSERT_EQUAL_INT(0, tcgetattr(STDIN_FILENO, &after));
	TEST_ASSERT_EQUAL_UINT(before.c_lflag, after.c_lflag);
}

int
main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_init_disables_icanon_and_echo);
	RUN_TEST(test_shutdown_restores_original_settings);
	RUN_TEST(
	    test_init_is_idempotent_with_shutdown_in_between);
	return UNITY_END();
}
