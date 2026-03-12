cmake_minimum_required(VERSION 3.15)
project(ghostim VERSION 2.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ── Find required libraries ───────────────────────────────────────────────
find_package(PkgConfig REQUIRED)

pkg_check_modules(JPEG  REQUIRED libjpeg)
pkg_check_modules(PNG   REQUIRED libpng)
pkg_check_modules(WEBP  REQUIRED libwebp)

# ── Sources ───────────────────────────────────────────────────────────────
set(SOURCES
    src/main.c
    src/args.c
    src/platform.c
    src/jpeg_parser.c
    src/png_parser.c
    src/webp_parser.c
    src/batch.c
)

add_executable(ghostim ${SOURCES})

# ── Include dirs ──────────────────────────────────────────────────────────
target_include_directories(ghostim PRIVATE
    include
    ${JPEG_INCLUDE_DIRS}
    ${PNG_INCLUDE_DIRS}
    ${WEBP_INCLUDE_DIRS}
)

# ── Link libraries ────────────────────────────────────────────────────────
# Use static libs when available, fallback to dynamic (required on macOS)
if(JPEG_STATIC_LIBRARIES)
    target_link_libraries(ghostim PRIVATE ${JPEG_STATIC_LIBRARIES})
else()
    target_link_libraries(ghostim PRIVATE ${JPEG_LIBRARIES})
endif()
if(PNG_STATIC_LIBRARIES)
    target_link_libraries(ghostim PRIVATE ${PNG_STATIC_LIBRARIES})
else()
    target_link_libraries(ghostim PRIVATE ${PNG_LIBRARIES})
endif()
if(WEBP_STATIC_LIBRARIES)
    target_link_libraries(ghostim PRIVATE ${WEBP_STATIC_LIBRARIES})
else()
    target_link_libraries(ghostim PRIVATE ${WEBP_LIBRARIES})
endif()

# static-libgcc only on Linux/MinGW — clang/macOS does not support it
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND NOT APPLE)
    target_link_options(ghostim PRIVATE -static-libgcc)
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

# ── Sanitizers on Debug ───────────────────────────────────────────────────
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

include(GNUInstallDirs)
install(TARGETS ghostim RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

message(STATUS "ghostim ${PROJECT_VERSION}  |  ${CMAKE_SYSTEM_NAME}  |  ${CMAKE_BUILD_TYPE}")
message(STATUS "  libjpeg : ${JPEG_VERSION}")
message(STATUS "  libpng  : ${PNG_VERSION}")
message(STATUS "  libwebp : ${WEBP_VERSION}")
