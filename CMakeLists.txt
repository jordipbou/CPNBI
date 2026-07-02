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

# Unit tests

## Test PTY

add_executable(test_pty_io tests/test_pty_io.c)
target_link_libraries(test_pty_io PRIVATE 
	cpnbi_lib unity util pthread)

## Test decoder

add_executable(test_decoder tests/test_decoder.c)
target_link_libraries(test_decoder PRIVATE cpnbi_lib unity)

## Test termios lifecycle

add_executable(test_termios_lifecycle 
	tests/test_termios_lifecycle)
target_link_libraries(test_termios_lifecycle PRIVATE 
	cpnbi_lib 
	unity
	util)

## Test signal handling

add_executable(test_signal_handling
	tests/test_signal_handling.c)
target_link_libraries(test_signal_handling PRIVATE
	cpnbi_lib
	unity
	util)

# Adding tests to ctest

enable_testing()
add_test(NAME pty_io COMMAND test_pty_io)
add_test(NAME decoder COMMAND test_decoder)
add_test(NAME termios_lifecycle COMMAND test_termios_lifecycle)
add_test(NAME signal_handling COMMAND test_signal_handling)
