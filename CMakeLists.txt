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

# Test PTY

add_executable(test_pty_io tests/test_pty_io.c)
target_link_libraries(test_pty_io PRIVATE cpnbi_lib unity util)
enable_testing()
add_test(NAME pty_io COMMAND test_pty_io)

# Test executable

add_executable(demo demo.c)
target_link_libraries(demo cpnbi_lib)
