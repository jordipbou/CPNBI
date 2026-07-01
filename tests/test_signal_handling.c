/* tests/test_signal_handling.c
 *
 * Verifies that cpnbi_init()'s SIGINT/SIGTERM handlers 
 * restore the terminal before the process dies (point 4, 
 * abrupt-termination half).
 * Linux-only: forks a real child process and sends it a 
 * real signal, since this behavior can't be exercised 
 * any other way.
 */
#include "cpnbi.h"
#include "unity.h"

#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif

static int master_fd;
static int slave_fd;

void
setUp(void) {
	TEST_ASSERT_EQUAL_INT_MESSAGE(
	    0, openpty(&master_fd, &slave_fd, NULL, NULL, NULL),
	    "openpty() failed - is a pty available in this "
	    "environment?");
}

void
tearDown(void) {
	close(master_fd);
	close(slave_fd);
}

/* Forks a child that redirects stdin to the pty slave, 
	 calls cpnbi_init(), signals readiness through `ready_pipe`,
	 then blocks until killed by `signum`. Returns the child's 
	 pid; caller is responsible for kill() and waitpid(). */
static pid_t
spawn_child_waiting_for_signal(int ready_write_fd) {
	pid_t pid = fork();
	TEST_ASSERT_TRUE_MESSAGE(pid >= 0, "fork() failed");

	if (pid == 0) {
		/* child */
		dup2(slave_fd, STDIN_FILENO);
		cpnbi_init();

		/* tell the parent we're ready to be signalled */
		char byte = 1;
		write(ready_write_fd, &byte, 1);
		close(ready_write_fd);

		pause();  /* block until a signal arrives */
		_exit(1); /* should never get here */
	}

	return pid;
}

static void
run_signal_test(int signum) {
	struct termios before, after;
	int ready_pipe[2];
	pid_t pid;
	int status;
	char byte;

	TEST_ASSERT_EQUAL_INT(0, tcgetattr(master_fd, &before));
	TEST_ASSERT_EQUAL_INT(0, pipe(ready_pipe));

	pid = spawn_child_waiting_for_signal(ready_pipe[1]);
	close(ready_pipe[1]); /* parent doesn't write */

	/* block until the child has actually finished 
			 cpnbi_init() - avoids a racy sleep()-based 
			 synchronization */
	TEST_ASSERT_EQUAL_INT(1, read(ready_pipe[0], &byte, 1));
	close(ready_pipe[0]);

	TEST_ASSERT_EQUAL_INT(0, kill(pid, signum));
	TEST_ASSERT_EQUAL_INT(pid, waitpid(pid, &status, 0));

	TEST_ASSERT_TRUE_MESSAGE(
	    WIFSIGNALED(status),
	    "child should have been terminated by the signal, "
	    "not exited normally");
	TEST_ASSERT_EQUAL_INT(signum, WTERMSIG(status));

	/* Terminal state lives with the pty pair, queryable 
			 from the master side even after the slave-side 
			 process is gone. */
	TEST_ASSERT_EQUAL_INT(0, tcgetattr(master_fd, &after));
	TEST_ASSERT_EQUAL_UINT_MESSAGE(
	    before.c_lflag, after.c_lflag,
	    "terminal was not restored before the process died");
}

void
test_sigterm_restores_terminal(void) {
	run_signal_test(SIGTERM);
}

void
test_sigint_restores_terminal(void) {
	run_signal_test(SIGINT);
}

int
main(void) {
	UNITY_BEGIN();
	RUN_TEST(test_sigterm_restores_terminal);
	RUN_TEST(test_sigint_restores_terminal);
	return UNITY_END();
}
