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

# ── Static linking — portable binary, no runtime deps for end user ────────
if(NOT MSVC)
    target_link_options(ghostim PRIVATE -static-libgcc)
endif()

target_link_libraries(ghostim PRIVATE
    ${JPEG_STATIC_LIBRARIES}
    ${PNG_STATIC_LIBRARIES}
    ${WEBP_STATIC_LIBRARIES}
)

# Fallback to dynamic if static not available (e.g. macOS)
if(NOT JPEG_STATIC_LIBRARIES)
    target_link_libraries(ghostim PRIVATE ${JPEG_LIBRARIES})
endif()
if(NOT PNG_STATIC_LIBRARIES)
    target_link_libraries(ghostim PRIVATE ${PNG_LIBRARIES})
endif()
if(NOT WEBP_STATIC_LIBRARIES)
    target_link_libraries(ghostim PRIVATE ${WEBP_LIBRARIES})
endif()

# ── Compiler warnings ─────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(ghostim PRIVATE /W4 /WX)
else()
    target_compile_options(ghostim PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow
        -Wformat=2 -Wconversion -Wsign-conversion
        -Wstrict-prototypes -Wmissing-prototypes
    )
endif()

# ── Sanitizers on Debug ───────────────────────────────────────────────────
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(NOT MSVC AND NOT WIN32)
        target_compile_options(ghostim PRIVATE -fsanitize=address,undefined)
        target_link_options(ghostim PRIVATE -fsanitize=address,undefined)
    endif()
endif()

# ── Release: optimize + strip ─────────────────────────────────────────────
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(NOT MSVC)
        target_compile_options(ghostim PRIVATE -O2)
        if(NOT APPLE)
            target_link_options(ghostim PRIVATE -s)
        endif()
    endif()
endif()

# ── Windows static runtime ────────────────────────────────────────────────
if(MSVC)
    set_property(TARGET ghostim PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

include(GNUInstallDirs)
install(TARGETS ghostim RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

message(STATUS "ghostim ${PROJECT_VERSION}  |  ${CMAKE_SYSTEM_NAME}  |  ${CMAKE_BUILD_TYPE}")
message(STATUS "  libjpeg : ${JPEG_VERSION}")
message(STATUS "  libpng  : ${PNG_VERSION}")
message(STATUS "  libwebp : ${WEBP_VERSION}")
