cmake_minimum_required(VERSION 3.15)
project(ghostim VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(SOURCES
    src/main.c
    src/args.c
    src/platform.c
    src/jpeg_parser.c
    src/png_parser.c
    src/batch.c
)

add_executable(ghostim ${SOURCES})

# Headers live in include/ — referenced as "ghostim/header.h"
target_include_directories(ghostim PRIVATE include)

# Warnings
if(MSVC)
    target_compile_options(ghostim PRIVATE /W4 /WX)
else()
    target_compile_options(ghostim PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow
        -Wformat=2 -Wconversion -Wsign-conversion
        -Wstrict-prototypes -Wmissing-prototypes
    )
endif()

# Sanitizers on Debug
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(NOT MSVC AND NOT WIN32)
        target_compile_options(ghostim PRIVATE -fsanitize=address,undefined)
        target_link_options(ghostim PRIVATE -fsanitize=address,undefined)
    endif()
endif()

# Release: optimize + strip symbols
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(NOT MSVC)
        target_compile_options(ghostim PRIVATE -O2)
        if(NOT APPLE)
            target_link_options(ghostim PRIVATE -s)
        endif()
    endif()
endif()

# Windows: static runtime for portable binary
if(MSVC)
    set_property(TARGET ghostim PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

include(GNUInstallDirs)
install(TARGETS ghostim RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

message(STATUS "ghostim ${PROJECT_VERSION}  |  ${CMAKE_SYSTEM_NAME}  |  ${CMAKE_BUILD_TYPE}")
