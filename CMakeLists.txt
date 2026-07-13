cmake_minimum_required(VERSION 3.16)
project(CPNBI LANGUAGES C)

include(FetchContent)

# ThrowTheSwitch/Unity is the test suite used by Sloth
FetchContent_Declare(
	unity
	GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
	GIT_TAG bbf8f3728a937c7627b8094de7ae13559d220ed5	
)

FetchContent_MakeAvailable(unity)

# Static library

add_library(cpnbi_lib STATIC cpnbi.c)
target_include_directories(cpnbi_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/>
        $<INSTALL_INTERFACE:>  # for install(EXPORT ...) scenarios
)

if (NOT MSVC)
  target_compile_options(cpnbi_lib PRIVATE -Wall -Wextra)
else()
  target_compile_options(cpnbi_lib PRIVATE /W4)
endif()

# Demo executable

add_executable(demo demo.c)
target_link_libraries(demo cpnbi_lib)

# Unit tests (enabled on every platform; individual targets
# are guarded below by what their implementation can support)
enable_testing()

# Cross-platform: the decode state machine (cpnbi__decode_event)
# and the shared event accessors are platform-agnostic, so the
# decoder suite runs on both Linux and Windows.
add_executable(test_decoder tests/test_decoder.c)
target_link_libraries(test_decoder PRIVATE cpnbi_lib unity)
add_test(NAME decoder COMMAND test_decoder)

if (WIN32)

	# Windows-only: exercises cpnbi__process_event() directly
	# with constructed INPUT_RECORDs (no real console needed).
	add_executable(test_process_event
		tests/test_process_event.c)
	target_link_libraries(test_process_event PRIVATE
		cpnbi_lib
		unity)
	add_test(NAME process_event COMMAND test_process_event)

	# Windows-only: verifies redirected (piped) stdin flows
	# through the shared cpnbi__decode_event() exactly like
	# the Linux test_pipe_io suite.
	add_executable(test_pipe_io_win
		tests/test_pipe_io_win.c)
	target_link_libraries(test_pipe_io_win PRIVATE
		cpnbi_lib
		unity)
	add_test(NAME pipe_io_win COMMAND test_pipe_io_win)

else()

	# Linux-only: these depend on POSIX facilities (pty,
	# termios, signals, and byte-stream pipes) that have no
	# portable Windows equivalent yet.

	## Test PTY
	add_executable(test_pty_io tests/test_pty_io.c)
	target_link_libraries(test_pty_io PRIVATE 
		cpnbi_lib unity util pthread)
	add_test(NAME pty_io COMMAND test_pty_io)

	## Test pipe (non-TTY) input
	add_executable(test_pipe_io tests/test_pipe_io.c)
	target_link_libraries(test_pipe_io PRIVATE cpnbi_lib unity)
	add_test(NAME pipe_io COMMAND test_pipe_io)

	## Test termios lifecycle
	add_executable(test_termios_lifecycle 
		tests/test_termios_lifecycle.c)
	target_link_libraries(test_termios_lifecycle PRIVATE 
		cpnbi_lib 
		unity
		util)
	add_test(NAME termios_lifecycle COMMAND 
		test_termios_lifecycle)

	## Test signal handling
	add_executable(test_signal_handling
		tests/test_signal_handling.c)
	target_link_libraries(test_signal_handling PRIVATE
		cpnbi_lib
		unity
		util)
	add_test(NAME signal_handling COMMAND test_signal_handling)

endif()
