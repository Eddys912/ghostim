cmake_minimum_required(VERSION 3.15)
project(ghostim VERSION 2.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ── Find libraries ────────────────────────────────────────────────────────
# Uses CMake's built-in find modules — works on Linux, macOS, and Windows
# (MSYS2/MinGW) without pkg-config path issues.

find_package(JPEG REQUIRED)   # FindJPEG.cmake — libjpeg / libjpeg-turbo
find_package(PNG  REQUIRED)   # FindPNG.cmake  — libpng + zlib

# WebP: no standard CMake module, locate manually
find_path(WEBP_INCLUDE_DIR
    NAMES webp/encode.h webp/decode.h
    REQUIRED
)
find_library(WEBP_LIBRARY     NAMES webp     REQUIRED)
find_library(WEBPDEMUX_LIBRARY NAMES webpdemux)
find_library(WEBPMUX_LIBRARY   NAMES webpmux)
find_library(SHARPYUV_LIBRARY  NAMES sharpyuv)

# ── Sources ───────────────────────────────────────────────────────────────
add_executable(ghostim
    src/main.c
    src/args.c
    src/platform.c
    src/jpeg_parser.c
    src/png_parser.c
    src/webp_parser.c
    src/batch.c
)

# ── Includes ──────────────────────────────────────────────────────────────
target_include_directories(ghostim PRIVATE
    include
    ${JPEG_INCLUDE_DIRS}
    ${PNG_INCLUDE_DIRS}
    ${WEBP_INCLUDE_DIR}
)

# ── Link ──────────────────────────────────────────────────────────────────
target_link_libraries(ghostim PRIVATE
    ${JPEG_LIBRARIES}
    ${PNG_LIBRARIES}
    ${WEBP_LIBRARY}
)
if(WEBPDEMUX_LIBRARY)
    target_link_libraries(ghostim PRIVATE ${WEBPDEMUX_LIBRARY})
endif()
if(WEBPMUX_LIBRARY)
    target_link_libraries(ghostim PRIVATE ${WEBPMUX_LIBRARY})
endif()
if(SHARPYUV_LIBRARY)
    target_link_libraries(ghostim PRIVATE ${SHARPYUV_LIBRARY})
endif()

# ── Compiler warnings ─────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(ghostim PRIVATE /W4)
else()
    target_compile_options(ghostim PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow
        -Wformat=2 -Wconversion -Wsign-conversion
        -Wstrict-prototypes -Wmissing-prototypes
    )
endif()

# ── Sanitizers on Debug (Linux/macOS only) ────────────────────────────────
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MSVC AND NOT WIN32)
    target_compile_options(ghostim PRIVATE -fsanitize=address,undefined)
    target_link_options(ghostim PRIVATE -fsanitize=address,undefined)
endif()

# ── Release: optimize + strip ─────────────────────────────────────────────
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT MSVC)
    target_compile_options(ghostim PRIVATE -O2)
    if(NOT APPLE)
        target_link_options(ghostim PRIVATE -s)
    endif()
endif()

install(TARGETS ghostim RUNTIME DESTINATION bin)

message(STATUS "ghostim ${PROJECT_VERSION}  |  ${CMAKE_SYSTEM_NAME}  |  ${CMAKE_BUILD_TYPE}")
message(STATUS "  JPEG include : ${JPEG_INCLUDE_DIRS}")
message(STATUS "  WebP include : ${WEBP_INCLUDE_DIR}")
message(STATUS "  WebP lib     : ${WEBP_LIBRARY}")
