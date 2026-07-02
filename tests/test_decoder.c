/* tests/test_decoder.c
 *
 * Pure decode-logic tests for cpnbi__decode_event() 
 * (Linux escape-sequence state machine). 
 * No terminal, pty, or real I/O involved - next_byte() and
 * more_available() are both mocked, so these tests are fast 
 * and deterministic, and run the same in any CI environment.
 *
 * The modifier packing was switched from decimal offsets 
 * to bit flags (point 8) as part of the point 6 work.
 */
#include "cpnbi.h"
#include "unity.h"

/* Internal symbol, not part of the public header, */
/* declared here for testing purposes only. */
int cpnbi__decode_event(int (*next_byte)(void),
                        int (*more_available)(void));

#define PACK_EVENT(k, m) ((k) | ((m) << 16))

/* --- Mock infrastructure --- */

static const int* mock_bytes;
static size_t mock_bytes_len;
static size_t mock_bytes_idx;
static int mock_more_available_value;

static int
mock_next_byte(void) {
	if (mock_bytes_idx < mock_bytes_len) {
		return mock_bytes[mock_bytes_idx++];
	}
	/* Ran off the end of the scripted bytes - a real bug 
		 in the test itself (sequence too short) rather than 
		 something we want to silently paper over. */
	TEST_FAIL_MESSAGE(
	    "mock_next_byte() called past end of scripted bytes");
	return -1;
}

static int
mock_more_available(void) {
	return mock_more_available_value;
}

/* Scripts the next decode call: bytes to feed via 
	 next_byte(), and what more_available() should report 
	 (only consulted right after a lone 27). */
static void
script(const int* bytes, size_t len,
       int more_available_value) {
	mock_bytes = bytes;
	mock_bytes_len = len;
	mock_bytes_idx = 0;
	mock_more_available_value = more_available_value;
}

void
setUp(void) {}

void
tearDown(void) {}

/* --- Plain characters (no escape involved) --- */

void
test_plain_char_passes_through_unchanged(void) {
	static const int bytes[] = {'a'};
	/* more_available_value irrelevant here - first byte 
		 isn't 27, so the lone-Esc disambiguation branch is 
		 never entered. */
	script(bytes, 1, 0);
	TEST_ASSERT_EQUAL_INT(
	    'a', cpnbi__decode_event(mock_next_byte,
	                             mock_more_available));
}

/* --- Lone Esc vs. start of a sequence --- */

void
test_lone_escape_key(void) {
	static const int bytes[] = {27};
	script(
	    bytes, 1,
	    0); /* more_available() says "nothing more waiting" */
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_ESCAPE,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

/* --- Simple arrows / Home / End, no modifiers --- */

void
test_arrow_up(void) {
	static const int bytes[] = {27, '[', 'A'};
	script(bytes, 3, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_UP,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

void
test_arrow_down(void) {
	static const int bytes[] = {27, '[', 'B'};
	script(bytes, 3, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_DOWN,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

void
test_arrow_left(void) {
	static const int bytes[] = {27, '[', 'D'};
	script(bytes, 3, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_LEFT,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

void
test_arrow_right(void) {
	static const int bytes[] = {27, '[', 'C'};
	script(bytes, 3, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_RIGHT,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

void
test_home_short_form(void) {
	static const int bytes[] = {27, '[', 'H'};
	script(bytes, 3, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_HOME,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

void
test_end_short_form(void) {
	static const int bytes[] = {27, '[', 'F'};
	script(bytes, 3, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_END,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

void
test_home_long_form(void) {
	static const int bytes[] = {27, '[', '1', '~'};
	script(bytes, 4, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_HOME,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

/* --- F1-F4 via SS3 (ESC O x) --- */

void
test_f1_ss3(void) {
	static const int bytes[] = {27, 'O', 'P'};
	script(bytes, 3, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_F1,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

void
test_f4_ss3(void) {
	static const int bytes[] = {27, 'O', 'S'};
	script(bytes, 3, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_F4,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

/* --- F5-F8 via ESC [ 1n~ --- */

void
test_f5(void) {
	static const int bytes[] = {27, '[', '1', '5', '~'};
	script(bytes, 5, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_F5,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

void
test_f8(void) {
	static const int bytes[] = {27, '[', '1', '9', '~'};
	script(bytes, 5, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_F8,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

/* --- Insert / Delete / PageUp / PageDown --- */

void
test_insert(void) {
	static const int bytes[] = {27, '[', '2', '~'};
	script(bytes, 4, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_INSERT,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

void
test_delete(void) {
	static const int bytes[] = {27, '[', '3', '~'};
	script(bytes, 4, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_DELETE,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

void
test_page_up(void) {
	static const int bytes[] = {27, '[', '5', '~'};
	script(bytes, 4, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_PAGE_UP,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

void
test_page_down(void) {
	static const int bytes[] = {27, '[', '6', '~'};
	script(bytes, 4, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_PAGE_DOWN,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

/* --- F9-F12 via ESC [ 2n~ --- */

void
test_f9(void) {
	static const int bytes[] = {27, '[', '2', '0', '~'};
	script(bytes, 5, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_F9,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

void
test_f12(void) {
	static const int bytes[] = {27, '[', '2', '4', '~'};
	script(bytes, 5, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_F12,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

/* --- Modifiers on arrows: ESC [ 1 ; N letter --- */

void
test_ctrl_up(void) {
	static const int bytes[] = {27, '[', '1', ';', '5', 'A'};
	script(bytes, 6, 1);
	TEST_ASSERT_EQUAL_INT(
	    PACK_EVENT(CPNBI_KEY_UP, CPNBI_MOD_CTRL),
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

void
test_shift_left(void) {
	static const int bytes[] = {27, '[', '1', ';', '2', 'D'};
	script(bytes, 6, 1);
	TEST_ASSERT_EQUAL_INT(
	    PACK_EVENT(CPNBI_KEY_LEFT, CPNBI_MOD_SHIFT),
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

void
test_alt_right(void) {
	static const int bytes[] = {27, '[', '1', ';', '3', 'C'};
	script(bytes, 6, 1);
	TEST_ASSERT_EQUAL_INT(
	    PACK_EVENT(CPNBI_KEY_RIGHT, CPNBI_MOD_ALT),
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

void
test_shift_ctrl_alt_down(void) {
	static const int bytes[] = {27, '[', '1', ';', '8', 'B'};
	script(bytes, 6, 1);
	TEST_ASSERT_EQUAL_INT(
	    PACK_EVENT(CPNBI_KEY_DOWN,
	               CPNBI_MOD_SHIFT | CPNBI_MOD_CTRL
	                   | CPNBI_MOD_ALT),
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

/* --- Modifiers on Insert/Delete/PageUp/PageDown/End: 
	 ESC [ n ; N ~ --- */

void
test_ctrl_delete(void) {
	static const int bytes[] = {27, '[', '3', ';', '5', '~'};
	script(bytes, 6, 1);
	TEST_ASSERT_EQUAL_INT(
	    PACK_EVENT(CPNBI_KEY_DELETE, CPNBI_MOD_CTRL),
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

void
test_shift_page_down(void) {
	static const int bytes[] = {27, '[', '6', ';', '2', '~'};
	script(bytes, 6, 1);
	TEST_ASSERT_EQUAL_INT(
	    PACK_EVENT(CPNBI_KEY_PAGE_DOWN, CPNBI_MOD_SHIFT),
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

/* --- rxvt non-xterm sequences that should work unconditionally --- */

void
test_rxvt_home(void) {
	static const int bytes[] = {27, '[', '7', '~'};
	script(bytes, 4, 1);
	TEST_ASSERT_EQUAL_INT(
	    PACK_EVENT(CPNBI_KEY_HOME, CPNBI_MOD_NONE),
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

void
test_rxvt_end(void) {
	static const int bytes[] = {27, '[', '8', '~'};
	script(bytes, 4, 1);
	TEST_ASSERT_EQUAL_INT(
	    PACK_EVENT(CPNBI_KEY_END, CPNBI_MOD_NONE),
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

/* --- cpnbi_event_key / cpnbi_event_mod round-trip --- */

void
test_accessors_round_trip_plain_char(void) {
	int event = PACK_EVENT('a', CPNBI_MOD_NONE);
	TEST_ASSERT_EQUAL_INT('a', cpnbi_event_key(event));
	TEST_ASSERT_EQUAL_INT(CPNBI_MOD_NONE,
	                      cpnbi_event_mod(event));
}

void
test_accessors_round_trip_shifted_key(void) {
	int event = PACK_EVENT(CPNBI_KEY_UP,
	                       CPNBI_MOD_SHIFT | CPNBI_MOD_CTRL);
	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_UP,
	                      cpnbi_event_key(event));
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_MOD_SHIFT | CPNBI_MOD_CTRL,
	    cpnbi_event_mod(event));
}

void
test_unrecognized_sequence_reports_nul(void) {
	static const int bytes[] = {
	    27, '[', 'Z'}; /* 'Z' isn't mapped anywhere */
	script(bytes, 3, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_NUL,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available));
}

int
main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_plain_char_passes_through_unchanged);

	RUN_TEST(test_lone_escape_key);

	RUN_TEST(test_arrow_up);
	RUN_TEST(test_arrow_down);
	RUN_TEST(test_arrow_left);
	RUN_TEST(test_arrow_right);
	RUN_TEST(test_home_short_form);
	RUN_TEST(test_end_short_form);
	RUN_TEST(test_home_long_form);

	RUN_TEST(test_f1_ss3);
	RUN_TEST(test_f4_ss3);
	RUN_TEST(test_f5);
	RUN_TEST(test_f8);

	RUN_TEST(test_insert);
	RUN_TEST(test_delete);
	RUN_TEST(test_page_up);
	RUN_TEST(test_page_down);

	RUN_TEST(test_f9);
	RUN_TEST(test_f12);

	RUN_TEST(test_ctrl_up);
	RUN_TEST(test_shift_left);
	RUN_TEST(test_alt_right);
	RUN_TEST(test_shift_ctrl_alt_down);

	RUN_TEST(test_ctrl_delete);
	RUN_TEST(test_shift_page_down);

	RUN_TEST(test_unrecognized_sequence_reports_nul);

	RUN_TEST(test_rxvt_home);
	RUN_TEST(test_rxvt_end);

	RUN_TEST(test_accessors_round_trip_plain_char);
	RUN_TEST(test_accessors_round_trip_shifted_key);

	return UNITY_END();
}
