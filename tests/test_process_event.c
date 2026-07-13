/* tests/test_process_event.c
 *
 * Windows-side analog of tests/test_decoder.c: exercises
 * cpnbi__process_event(INPUT_RECORD*) directly with
 * constructed console input records - no real console,
 * window, or human interaction required. Covers the
 * same key/modifier matrix the Linux decoder test does,
 * plus the surrogate-pair pass-through that cpnbi_get_event()
 * reassembles.
 *
 * Build only on Windows (cpnbi__process_event and
 * INPUT_RECORD are Windows-only).
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "cpnbi.h"
#include "unity.h"

/* Internal symbol, not part of the public header, */
/* declared here for testing purposes only. */
int32_t cpnbi__process_event(INPUT_RECORD* record);

#define PACK_EVENT(k, m) ((k) | ((m) << CPNBI_MOD_OFFSET))

/* --- Mock infrastructure --- */

static INPUT_RECORD
make_key(WORD vk, DWORD ctrl, WCHAR wc, BOOL down) {
	INPUT_RECORD r;

	memset(&r, 0, sizeof(r));
	r.EventType = KEY_EVENT;
	r.Event.KeyEvent.bKeyDown = down;
	r.Event.KeyEvent.wRepeatCount = 1;
	r.Event.KeyEvent.wVirtualKeyCode = vk;
	r.Event.KeyEvent.wVirtualScanCode = 0;
	r.Event.KeyEvent.uChar.UnicodeChar = wc;
	r.Event.KeyEvent.dwControlKeyState = ctrl;

	return r;
}

void
setUp(void) {}

void
tearDown(void) {}

/* --- Plain characters --- */

void
test_process_event_printable_char(void) {
	INPUT_RECORD r = make_key(0, CPNBI_MOD_NONE, L'a', TRUE);

	TEST_ASSERT_EQUAL_INT('a', cpnbi__process_event(&r));
}

void
test_process_event_unicode_char(void) {
	INPUT_RECORD r = make_key(0, CPNBI_MOD_NONE, 0x00F1, TRUE);

	TEST_ASSERT_EQUAL_INT(0x00F1, cpnbi__process_event(&r));
}

/* --- Non-key / key-up records are ignored (return 0) --- */

void
test_process_event_key_up_ignored(void) {
	INPUT_RECORD r = make_key(VK_UP, CPNBI_MOD_NONE, 0, FALSE);

	TEST_ASSERT_EQUAL_INT(0, cpnbi__process_event(&r));
}

void
test_process_event_non_key_record_ignored(void) {
	INPUT_RECORD r;

	memset(&r, 0, sizeof(r));
	r.EventType = MOUSE_EVENT;

	TEST_ASSERT_EQUAL_INT(0, cpnbi__process_event(&r));
}

/* --- Arrows, no modifiers --- */

void
test_process_event_up(void) {
	INPUT_RECORD r = make_key(VK_UP, CPNBI_MOD_NONE, 0, TRUE);

	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_UP,
	                      cpnbi__process_event(&r));
}

void
test_process_event_down(void) {
	INPUT_RECORD r = make_key(VK_DOWN, CPNBI_MOD_NONE, 0, TRUE);

	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_DOWN,
	                      cpnbi__process_event(&r));
}

void
test_process_event_left(void) {
	INPUT_RECORD r = make_key(VK_LEFT, CPNBI_MOD_NONE, 0, TRUE);

	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_LEFT,
	                      cpnbi__process_event(&r));
}

void
test_process_event_right(void) {
	INPUT_RECORD r = make_key(VK_RIGHT, CPNBI_MOD_NONE, 0, TRUE);

	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_RIGHT,
	                      cpnbi__process_event(&r));
}

/* --- Home / End / Insert / Delete / PageUp / PageDown --- */

void
test_process_event_home(void) {
	INPUT_RECORD r = make_key(VK_HOME, CPNBI_MOD_NONE, 0, TRUE);

	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_HOME,
	                      cpnbi__process_event(&r));
}

void
test_process_event_end(void) {
	INPUT_RECORD r = make_key(VK_END, CPNBI_MOD_NONE, 0, TRUE);

	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_END,
	                      cpnbi__process_event(&r));
}

void
test_process_event_insert(void) {
	INPUT_RECORD r = make_key(VK_INSERT, CPNBI_MOD_NONE, 0, TRUE);

	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_INSERT,
	                      cpnbi__process_event(&r));
}

void
test_process_event_delete(void) {
	INPUT_RECORD r = make_key(VK_DELETE, CPNBI_MOD_NONE, 0, TRUE);

	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_DELETE,
	                      cpnbi__process_event(&r));
}

void
test_process_event_page_up(void) {
	INPUT_RECORD r = make_key(VK_PRIOR, CPNBI_MOD_NONE, 0, TRUE);

	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_PAGE_UP,
	                      cpnbi__process_event(&r));
}

void
test_process_event_page_down(void) {
	INPUT_RECORD r = make_key(VK_NEXT, CPNBI_MOD_NONE, 0, TRUE);

	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_PAGE_DOWN,
	                      cpnbi__process_event(&r));
}

/* --- F1-F12 --- */

void
test_process_event_f1(void) {
	INPUT_RECORD r = make_key(VK_F1, CPNBI_MOD_NONE, 0, TRUE);

	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_F1,
	                      cpnbi__process_event(&r));
}

void
test_process_event_f12(void) {
	INPUT_RECORD r = make_key(VK_F12, CPNBI_MOD_NONE, 0, TRUE);

	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_F12,
	                      cpnbi__process_event(&r));
}

/* --- Escape / Enter / Backspace / Tab --- */

void
test_process_event_escape(void) {
	INPUT_RECORD r = make_key(VK_ESCAPE, CPNBI_MOD_NONE, 0, TRUE);

	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_ESCAPE,
	                      cpnbi__process_event(&r));
}

void
test_process_event_enter(void) {
	INPUT_RECORD r = make_key(VK_RETURN, CPNBI_MOD_NONE, 0, TRUE);

	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_ENTER,
	                      cpnbi__process_event(&r));
}

void
test_process_event_backspace(void) {
	INPUT_RECORD r = make_key(VK_BACK, CPNBI_MOD_NONE, 0, TRUE);

	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_BACKSPACE,
	                      cpnbi__process_event(&r));
}

void
test_process_event_tab(void) {
	INPUT_RECORD r = make_key(VK_TAB, CPNBI_MOD_NONE, 0, TRUE);

	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_TAB,
	                      cpnbi__process_event(&r));
}

/* --- Modifiers --- */

void
test_process_event_shift_up(void) {
	INPUT_RECORD r =
	    make_key(VK_UP, SHIFT_PRESSED, 0, TRUE);

	TEST_ASSERT_EQUAL_INT(
	    PACK_EVENT(CPNBI_KEY_UP, CPNBI_MOD_SHIFT),
	    cpnbi__process_event(&r));
}

void
test_process_event_ctrl_down(void) {
	INPUT_RECORD r =
	    make_key(VK_DOWN, LEFT_CTRL_PRESSED, 0, TRUE);

	TEST_ASSERT_EQUAL_INT(
	    PACK_EVENT(CPNBI_KEY_DOWN, CPNBI_MOD_CTRL),
	    cpnbi__process_event(&r));
}

void
test_process_event_alt_right(void) {
	INPUT_RECORD r =
	    make_key(VK_RIGHT, RIGHT_ALT_PRESSED, 0, TRUE);

	TEST_ASSERT_EQUAL_INT(
	    PACK_EVENT(CPNBI_KEY_RIGHT, CPNBI_MOD_ALT),
	    cpnbi__process_event(&r));
}

void
test_process_event_shift_ctrl_alt_left(void) {
	INPUT_RECORD r = make_key(
	    VK_LEFT,
	    SHIFT_PRESSED | LEFT_CTRL_PRESSED | LEFT_ALT_PRESSED,
	    0, TRUE);

	TEST_ASSERT_EQUAL_INT(
	    PACK_EVENT(CPNBI_KEY_LEFT,
	               CPNBI_MOD_SHIFT | CPNBI_MOD_CTRL
	                   | CPNBI_MOD_ALT),
	    cpnbi__process_event(&r));
}

void
test_process_event_right_ctrl_mod(void) {
	INPUT_RECORD r =
	    make_key(VK_UP, RIGHT_CTRL_PRESSED, 0, TRUE);

	TEST_ASSERT_EQUAL_INT(
	    PACK_EVENT(CPNBI_KEY_UP, CPNBI_MOD_CTRL),
	    cpnbi__process_event(&r));
}

/* --- cpnbi_event_key / cpnbi_event_mod round-trip --- */

void
test_process_event_accessors_shifted_key(void) {
	INPUT_RECORD r =
	    make_key(VK_F5, SHIFT_PRESSED | LEFT_CTRL_PRESSED, 0,
	             TRUE);
	int32_t event = cpnbi__process_event(&r);

	TEST_ASSERT_EQUAL_INT(CPNBI_KEY_F5,
	                      cpnbi_event_key(event));
	TEST_ASSERT_EQUAL_INT(CPNBI_MOD_SHIFT | CPNBI_MOD_CTRL,
	                      cpnbi_event_mod(event));
}

void
test_process_event_accessors_plain_char(void) {
	INPUT_RECORD r = make_key(0, CPNBI_MOD_NONE, L'a', TRUE);
	int32_t event = cpnbi__process_event(&r);

	TEST_ASSERT_EQUAL_INT('a', cpnbi_event_key(event));
	TEST_ASSERT_EQUAL_INT(CPNBI_MOD_NONE,
	                      cpnbi_event_mod(event));
}

/* --- Surrogate pair pass-through --- */
/* cpnbi__process_event returns the high surrogate as-is; */
/* cpnbi_get_event() reassembles it with the following low */
/* surrogate record. */

void
test_process_event_high_surrogate_passthrough(void) {
	INPUT_RECORD r =
	    make_key(0, CPNBI_MOD_NONE, 0xD83D, TRUE);

	TEST_ASSERT_EQUAL_INT(0xD83D, cpnbi__process_event(&r));
}

int
main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_process_event_printable_char);
	RUN_TEST(test_process_event_unicode_char);

	RUN_TEST(test_process_event_key_up_ignored);
	RUN_TEST(test_process_event_non_key_record_ignored);

	RUN_TEST(test_process_event_up);
	RUN_TEST(test_process_event_down);
	RUN_TEST(test_process_event_left);
	RUN_TEST(test_process_event_right);

	RUN_TEST(test_process_event_home);
	RUN_TEST(test_process_event_end);
	RUN_TEST(test_process_event_insert);
	RUN_TEST(test_process_event_delete);
	RUN_TEST(test_process_event_page_up);
	RUN_TEST(test_process_event_page_down);

	RUN_TEST(test_process_event_f1);
	RUN_TEST(test_process_event_f12);

	RUN_TEST(test_process_event_escape);
	RUN_TEST(test_process_event_enter);
	RUN_TEST(test_process_event_backspace);
	RUN_TEST(test_process_event_tab);

	RUN_TEST(test_process_event_shift_up);
	RUN_TEST(test_process_event_ctrl_down);
	RUN_TEST(test_process_event_alt_right);
	RUN_TEST(test_process_event_shift_ctrl_alt_left);
	RUN_TEST(test_process_event_right_ctrl_mod);

	RUN_TEST(test_process_event_accessors_shifted_key);
	RUN_TEST(test_process_event_accessors_plain_char);

	RUN_TEST(test_process_event_high_surrogate_passthrough);

	return UNITY_END();
}
