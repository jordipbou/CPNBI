cmake_minimum_required(VERSION 3.16)
project(CPNBI LANGUAGES C)

# Static library

add_library(cpnbi_lib STATIC cpnbi.c)
target_include_directories(cpnbi_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/>
        $<INSTALL_INTERFACE:>  # for install(EXPORT ...) scenarios
)

add_alias(cpnbi::cpnbi_lib ALIAS cpnbi_lib)

# Test executable

add_executable(test test.c)
target_link_libraries(test cpnbi_lib)
