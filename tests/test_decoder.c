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
int32_t cpnbi__decode_event(int (*next_byte)(void),
                            int (*more_available)(void),
                            int utf8);

#define PACK_EVENT(k, m) ((k) | ((m) << CPNBI_MOD_OFFSET))

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
	                             mock_more_available, 1));
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
	                        mock_more_available, 1));
}

/* --- Simple arrows / Home / End, no modifiers --- */

void
test_arrow_up(void) {
	static const int bytes[] = {27, '[', 'A'};
	script(bytes, 3, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_UP,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
}

void
test_arrow_down(void) {
	static const int bytes[] = {27, '[', 'B'};
	script(bytes, 3, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_DOWN,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
}

void
test_arrow_left(void) {
	static const int bytes[] = {27, '[', 'D'};
	script(bytes, 3, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_LEFT,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
}

void
test_arrow_right(void) {
	static const int bytes[] = {27, '[', 'C'};
	script(bytes, 3, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_RIGHT,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
}

void
test_home_short_form(void) {
	static const int bytes[] = {27, '[', 'H'};
	script(bytes, 3, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_HOME,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
}

void
test_end_short_form(void) {
	static const int bytes[] = {27, '[', 'F'};
	script(bytes, 3, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_END,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
}

void
test_home_long_form(void) {
	static const int bytes[] = {27, '[', '1', '~'};
	script(bytes, 4, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_HOME,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
}

/* --- F1-F4 via SS3 (ESC O x) --- */

void
test_f1_ss3(void) {
	static const int bytes[] = {27, 'O', 'P'};
	script(bytes, 3, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_F1,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
}

void
test_f4_ss3(void) {
	static const int bytes[] = {27, 'O', 'S'};
	script(bytes, 3, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_F4,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
}

/* --- F5-F8 via ESC [ 1n~ --- */

void
test_f5(void) {
	static const int bytes[] = {27, '[', '1', '5', '~'};
	script(bytes, 5, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_F5,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
}

void
test_f8(void) {
	static const int bytes[] = {27, '[', '1', '9', '~'};
	script(bytes, 5, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_F8,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
}

/* --- Insert / Delete / PageUp / PageDown --- */

void
test_insert(void) {
	static const int bytes[] = {27, '[', '2', '~'};
	script(bytes, 4, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_INSERT,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
}

void
test_delete(void) {
	static const int bytes[] = {27, '[', '3', '~'};
	script(bytes, 4, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_DELETE,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
}

void
test_page_up(void) {
	static const int bytes[] = {27, '[', '5', '~'};
	script(bytes, 4, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_PAGE_UP,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
}

void
test_page_down(void) {
	static const int bytes[] = {27, '[', '6', '~'};
	script(bytes, 4, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_PAGE_DOWN,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
}

/* --- F9-F12 via ESC [ 2n~ --- */

void
test_f9(void) {
	static const int bytes[] = {27, '[', '2', '0', '~'};
	script(bytes, 5, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_F9,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
}

void
test_f12(void) {
	static const int bytes[] = {27, '[', '2', '4', '~'};
	script(bytes, 5, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_F12,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
}

/* --- Modifiers on arrows: ESC [ 1 ; N letter --- */

void
test_ctrl_up(void) {
	static const int bytes[] = {27, '[', '1', ';', '5', 'A'};
	script(bytes, 6, 1);
	TEST_ASSERT_EQUAL_INT(
	    PACK_EVENT(CPNBI_KEY_UP, CPNBI_MOD_CTRL),
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
}

void
test_shift_left(void) {
	static const int bytes[] = {27, '[', '1', ';', '2', 'D'};
	script(bytes, 6, 1);
	TEST_ASSERT_EQUAL_INT(
	    PACK_EVENT(CPNBI_KEY_LEFT, CPNBI_MOD_SHIFT),
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
}

void
test_alt_right(void) {
	static const int bytes[] = {27, '[', '1', ';', '3', 'C'};
	script(bytes, 6, 1);
	TEST_ASSERT_EQUAL_INT(
	    PACK_EVENT(CPNBI_KEY_RIGHT, CPNBI_MOD_ALT),
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
}

void
test_shift_ctrl_alt_down(void) {
	static const int bytes[] = {27, '[', '1', ';', '8', 'B'};
	script(bytes, 6, 1);
	TEST_ASSERT_EQUAL_INT(
	    PACK_EVENT(CPNBI_KEY_DOWN, CPNBI_MOD_SHIFT
	                                   | CPNBI_MOD_CTRL
	                                   | CPNBI_MOD_ALT),
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
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
	                        mock_more_available, 1));
}

void
test_shift_page_down(void) {
	static const int bytes[] = {27, '[', '6', ';', '2', '~'};
	script(bytes, 6, 1);
	TEST_ASSERT_EQUAL_INT(
	    PACK_EVENT(CPNBI_KEY_PAGE_DOWN, CPNBI_MOD_SHIFT),
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
}

/* --- get_char loop behavior: non-printable bytes are */
/*     skipped, the decoder advances past them          */
/*     atomically without losing position                */

void
test_get_char_loop_skips_non_printable_then_finds_printable(
    void) {
	/* Mimics what cpnbi_get_char() does internally: */
	/* call cpnbi__decode_event() in a loop, ignoring */
	/* non-printable results until a printable one */
	/* appears.                                        */
	static const int bytes[] = {0x01, 'a'};
	int event;
	int key;

	script(bytes, 2, 0);
	event = cpnbi__decode_event(mock_next_byte,
	                            mock_more_available, 1);
	key = cpnbi_event_key(event);
	/* First call: non-printable byte (0x01), */
	/* key < 32, skipped by get_char's loop */
	TEST_ASSERT_TRUE_MESSAGE(
	    key < 32 || key > 126,
	    "first byte should not be printable");

	/* Second call: printable 'a' returned */
	event = cpnbi__decode_event(mock_next_byte,
	                            mock_more_available, 1);
	key = cpnbi_event_key(event);
	TEST_ASSERT_EQUAL_INT('a', key);
}

/* --- rxvt non-xterm sequences that should work unconditionally --- */

void
test_rxvt_home(void) {
	static const int bytes[] = {27, '[', '7', '~'};
	script(bytes, 4, 1);
	TEST_ASSERT_EQUAL_INT(
	    PACK_EVENT(CPNBI_KEY_HOME, CPNBI_MOD_NONE),
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
}

void
test_rxvt_end(void) {
	static const int bytes[] = {27, '[', '8', '~'};
	script(bytes, 4, 1);
	TEST_ASSERT_EQUAL_INT(
	    PACK_EVENT(CPNBI_KEY_END, CPNBI_MOD_NONE),
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
}

/* --- UTF-8 multi-byte decoding --- */

void
test_utf8_two_byte(void) {
	static const int bytes[] = {0xC3, 0xB1};
	script(bytes, 2, 1);
	TEST_ASSERT_EQUAL_INT(
	    0x00F1, cpnbi__decode_event(mock_next_byte,
	                                mock_more_available, 1));
}

void
test_utf8_three_byte(void) {
	static const int bytes[] = {0xE4, 0xBD, 0xA0};
	script(bytes, 3, 1);
	TEST_ASSERT_EQUAL_INT(
	    0x4F60, cpnbi__decode_event(mock_next_byte,
	                                mock_more_available, 1));
}

void
test_utf8_four_byte(void) {
	static const int bytes[] = {0xF0, 0x9F, 0x98, 0x82};
	script(bytes, 4, 1);
	TEST_ASSERT_EQUAL_INT(
	    0x1F602, cpnbi__decode_event(mock_next_byte,
	                                 mock_more_available, 1));
}

/* --- Raw mode: multi-byte sequences return individual bytes --- */

void
test_raw_two_byte_returns_separate_bytes(void) {
	static const int bytes[] = {0xC3, 0xB1};
	script(bytes, 2, 1);
	TEST_ASSERT_EQUAL_INT(
	    0xC3, cpnbi__decode_event(mock_next_byte,
	                              mock_more_available, 0));
	TEST_ASSERT_EQUAL_INT(
	    0xB1, cpnbi__decode_event(mock_next_byte,
	                              mock_more_available, 0));
}

void
test_raw_three_byte_returns_separate_bytes(void) {
	static const int bytes[] = {0xE4, 0xBD, 0xA0};
	script(bytes, 3, 1);
	TEST_ASSERT_EQUAL_INT(
	    0xE4, cpnbi__decode_event(mock_next_byte,
	                              mock_more_available, 0));
	TEST_ASSERT_EQUAL_INT(
	    0xBD, cpnbi__decode_event(mock_next_byte,
	                              mock_more_available, 0));
	TEST_ASSERT_EQUAL_INT(
	    0xA0, cpnbi__decode_event(mock_next_byte,
	                              mock_more_available, 0));
}

void
test_raw_four_byte_returns_separate_bytes(void) {
	static const int bytes[] = {0xF0, 0x9F, 0x98, 0x82};
	script(bytes, 4, 1);
	TEST_ASSERT_EQUAL_INT(
	    0xF0, cpnbi__decode_event(mock_next_byte,
	                              mock_more_available, 0));
	TEST_ASSERT_EQUAL_INT(
	    0x9F, cpnbi__decode_event(mock_next_byte,
	                              mock_more_available, 0));
	TEST_ASSERT_EQUAL_INT(
	    0x98, cpnbi__decode_event(mock_next_byte,
	                              mock_more_available, 0));
	TEST_ASSERT_EQUAL_INT(
	    0x82, cpnbi__decode_event(mock_next_byte,
	                              mock_more_available, 0));
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
	TEST_ASSERT_EQUAL_INT(CPNBI_MOD_SHIFT | CPNBI_MOD_CTRL,
	                      cpnbi_event_mod(event));
}

void
test_event_type_always_press(void) {
	int event = PACK_EVENT('a', CPNBI_MOD_NONE);
	TEST_ASSERT_EQUAL_INT(0, cpnbi_event_type(event));

	event = PACK_EVENT(CPNBI_KEY_UP, CPNBI_MOD_SHIFT);
	TEST_ASSERT_EQUAL_INT(0, cpnbi_event_type(event));
}

void
test_event_is_special_detects_unicode(void) {
	int event = PACK_EVENT('a', CPNBI_MOD_NONE);
	TEST_ASSERT_EQUAL_INT(0, cpnbi_event_is_special(event));

	event = PACK_EVENT(0x00F1, CPNBI_MOD_NONE);
	TEST_ASSERT_EQUAL_INT(0, cpnbi_event_is_special(event));

	event = PACK_EVENT(0x1F602, CPNBI_MOD_NONE);
	TEST_ASSERT_EQUAL_INT(0, cpnbi_event_is_special(event));
}

void
test_event_is_special_detects_special_keys(void) {
	int event = PACK_EVENT(CPNBI_KEY_UP, CPNBI_MOD_NONE);
	TEST_ASSERT_EQUAL_INT(1, cpnbi_event_is_special(event));

	event = PACK_EVENT(CPNBI_KEY_F1, CPNBI_MOD_SHIFT);
	TEST_ASSERT_EQUAL_INT(1, cpnbi_event_is_special(event));
}

void
test_unrecognized_sequence_reports_nul(void) {
	static const int bytes[] = {
	    27, '[', 'Z'}; /* 'Z' isn't mapped anywhere */
	script(bytes, 3, 1);
	TEST_ASSERT_EQUAL_INT(
	    CPNBI_KEY_NUL,
	    cpnbi__decode_event(mock_next_byte,
	                        mock_more_available, 1));
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

	RUN_TEST(
	    test_get_char_loop_skips_non_printable_then_finds_printable);

	RUN_TEST(test_rxvt_home);
	RUN_TEST(test_rxvt_end);

	RUN_TEST(test_utf8_two_byte);
	RUN_TEST(test_utf8_three_byte);
	RUN_TEST(test_utf8_four_byte);

	RUN_TEST(test_raw_two_byte_returns_separate_bytes);
	RUN_TEST(test_raw_three_byte_returns_separate_bytes);
	RUN_TEST(test_raw_four_byte_returns_separate_bytes);

	RUN_TEST(test_accessors_round_trip_plain_char);
	RUN_TEST(test_accessors_round_trip_shifted_key);
	RUN_TEST(test_event_type_always_press);
	RUN_TEST(test_event_is_special_detects_unicode);
	RUN_TEST(test_event_is_special_detects_special_keys);

	return UNITY_END();
}
