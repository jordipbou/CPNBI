# Testing CPNBI

This document covers how CPNBI is tested: the automated suites (Unity, run via `ctest`), what each one is responsible for, and the one thing that has to be checked by hand because it can't be automated reliably.

## Automated test suites

Build and run all of them with:

```bash
cmake -B build
cmake --build build
cd build && ctest --output-on-failure
```

Or build+run just the tests during local development:

```bash
cmake --build build --target run_tests
```

| Suite | File | Platform | What it covers |
|---|---|---|---|
| Decoder logic | `tests/test_decoder.c` | Linux + Windows (mocked) | The escape-sequence state machine (`cpnbi__decode_event`), tested via mocked `next_byte`/`more_available` functions - no terminal needed. Covers arrows, F-keys, Home/End/Insert/Delete/PageUp/PageDown, all modifier combinations, lone Esc, UTF-8, EOF, and unrecognized sequences. Also covers the shared `cpnbi_event_*` accessors. The decoder is platform-agnostic, so this suite runs on both toolchains. |
| Windows key mapping | `tests/test_process_event.c` | Windows | The Windows console path (`cpnbi__process_event`) driven by constructed `INPUT_RECORD`s - no real console needed. Mirrors the decoder suite's key/modifier matrix plus surrogate-pair pass-through and the ignored key-up / non-key records. |
| I/O plumbing | `tests/test_pty_io.c` | Linux (pty) | Byte-level correctness of `cpnbi_is_char_available()`/`cpnbi_get_event()` against a real pty. Includes the regression test for the original dropped-byte bug (point 1). |
| Termios lifecycle | `tests/test_termios_lifecycle.c` | Linux (pty) | Verifies `cpnbi_init()` puts the terminal into raw mode exactly once and `cpnbi_shutdown()` restores the original settings exactly (point 3). |
| Signal handling | `tests/test_signal_handling.c` | Linux (pty + fork) | Verifies SIGINT/SIGTERM are caught and the terminal is restored before the process dies, even on abrupt termination (point 4). |

All of the above run headless and are CI-safe (no physical terminal or human interaction required) - they use either mocked input or a real pty created programmatically via `openpty()`.

### Requirements

- A working `openpty()` (available on any normal Linux/macOS install and on GitHub Actions' `ubuntu-latest`/`macos-latest` runners). If `openpty()` fails in `setUp`, you'll get a clear assertion failure rather than a hang - this can happen in unusual sandboxed/containerized environments without pty support.
- Windows-side logic (`cpnbi__process_event`) is now covered by `tests/test_process_event.c`, which drives it with constructed `INPUT_RECORD`s and runs on Windows via the WSL-built Windows binaries (`ctest` under `build/windows/*`). The pty/termios/signal/pipe suites remain Linux-only.

## Manual verification: Windows Ctrl+C / console-close handling

Point 4's abrupt-termination handling on Linux (SIGINT/SIGTERM) has the automated `test_signal_handling.c` suite above.

The Windows equivalent (`SetConsoleCtrlHandler`, covering CTRL_C_EVENT, CTRL_BREAK_EVENT, CTRL_CLOSE_EVENT, CTRL_LOGOFF_EVENT, CTRL_SHUTDOWN_EVENT) is **not** covered by an automated test - reliably driving a real Windows
console from CI isn't practical for a library this size. Verify manually instead, after any change touching `cpnbi_init()`, `cpnbi_shutdown()`, or
`cpnbi__ctrl_handler` on Windows.

### Setup

Build and run any program that calls `cpnbi_init()` (the `test.c` sample, or your own app) directly in a real `cmd.exe` or Windows Terminal window - not piped/redirected, and not through an IDE's integrated terminal, since some of these don't forward console control events the same way a real console does.

### Checks

1. **Ctrl+C**
   - Start the program, let it call `cpnbi_init()` and sit waiting for input.
   - Press **Ctrl+C**.
   - Expected: the process exits, and the console returns to normal - typed characters are visible again (echo back on), and Enter produces a new line immediately rather than requiring an extra keypress. If the console looks "stuck" (no echo, or input feels unresponsive) after the process exits, `cpnbi_shutdown()` did not run - regression.

2. **Ctrl+Break**
   - Same as above, using **Ctrl+Break** instead of Ctrl+C.

3. **Closing the console window directly**
   - Start the program as above.
   - Click the window's **X** (close) button instead of pressing a key.
   - Expected: the window closes without hanging for several seconds (Windows force-kills the process a few seconds after `CTRL_CLOSE_EVENT` regardless, so a hang here would indicate the handler is doing something slow or blocking - worth investigating even though it's not directly a "terminal left broken" symptom).

4. **Sanity check after each of the above**
   - Type `echo hello` and press Enter in the same console window afterward.
   - Expected: normal shell behavior - text appears as you type, Enter runs the command immediately. This is the real-world symptom of a console left in a broken state (no ECHO / still in a raw-like mode) from an earlier session's window - if it looks wrong, the process that just exited did not restore console mode correctly.

### Not covered by this checklist

- **Logoff / system shutdown** (`CTRL_LOGOFF_EVENT`, `CTRL_SHUTDOWN_EVENT`) are impractical to test on demand without actually logging off or shutting down the machine. These are handled by the same code path as Ctrl+C/Close, so passing checks 1-3 above is reasonable indirect confidence, but isn't a substitute for the real thing if you have reason to suspect a regression specific to those paths.
