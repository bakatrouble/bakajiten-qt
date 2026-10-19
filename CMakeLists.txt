# Qt Widgets frontend for bakajiten.
#
# Layout: Rust builds `librecognizer` as a static library (via
# Corrosion); C++ owns the QMainWindow shell (dictionary central
# widget + floating handwriting panel) and calls the recognizer through
# the UniFFI C++ bindings (generated at build time, see
# cmake/uniffi.cmake and the README).
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/bakajiten            # run from ui_qt/ so ../../model.bin resolves
#
# Build logic lives in cmake/: dependencies (Qt, Rust), uniffi
# (binding generation), app (the bakajiten target), analysis
# (tidy/format gates), tests (QTest targets).

cmake_minimum_required(VERSION 3.24)

project(bakajiten_qt VERSION 0.1.0)

# Position matters: the Makefile generator snapshots this when each
# target is created, so it must precede every add_executable below
# (a set() after the target silently emits nothing).
# An empty (unset-equivalent) cache entry would otherwise shadow
# the set() — drop it so fresh and old build dirs behave the same.
# Explicit ON/OFF values are left alone.
if("${CMAKE_EXPORT_COMPILE_COMMANDS}" STREQUAL "")
    unset(CMAKE_EXPORT_COMPILE_COMMANDS CACHE)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# P2996 static reflection (GCC 16 snapshot: `^^`, access contexts)
# powers the generic dev fill in dev/uiform.h. Directory-wide (not
# per-target): anything including that header needs it, app and tests
# alike — and it must precede every add_executable below (directory
# options only affect later-created targets). Harmless elsewhere:
# plain GCC flag, accepted and ignored where unused. Only GCC 16+
# with C++26 implements it — any other compiler, older GCC, or older
# standard (e.g. CI runners on C++20) skips the flag; dev builds then
# fail loudly in uiform.h itself, while release builds never touch
# reflection (see the stub there). Lives after BAKAJITEN_CXX_STANDARD
# below so the guard can read it.

# Dev iteration: runtime .ui loading (QUiLoader) instead of compiled
# uic output, plus an F5 "Reload UI" action. Always on for Debug
# builds (a plain variable shadows any stale cache entry, so fossils
# can never silently drop it); -DDEV_UILOADER=ON enables it for other
# build types. To disable, configure a non-Debug build type without
# the flag.
option(DEV_UILOADER "Load Designer .ui files at runtime (dev iteration)" OFF)
if(CMAKE_BUILD_TYPE)
    string(TOUPPER "${CMAKE_BUILD_TYPE}" _DevUiLoaderBuildType)
    if(_DevUiLoaderBuildType STREQUAL "DEBUG")
        set(DEV_UILOADER ON)
    endif()
    unset(_DevUiLoaderBuildType)
endif()

include(cmake/dependencies.cmake)

# C++ standard for our targets — set only AFTER dependencies: language
# settings leak into try_compile checks inside find_package calls, and
# older toolchains cannot even generate test projects for newer
# dialects (observed: Qt6 dependency checks failing under MXE GCC).
# Overridable per toolchain (cross with an older compiler passes
# -DBAKAJITEN_CXX_STANDARD=20, as does CI on its older runners;
# P2996 reflection stays dev/GCC16-only regardless, see the flag
# guard below and uiform.h).
set(BAKAJITEN_CXX_STANDARD 26 CACHE STRING "C++ standard for bakajiten targets")
set(CMAKE_CXX_STANDARD ${BAKAJITEN_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The reflection flag itself (see above): directory-wide so every
# target created below inherits it, but only where the toolchain
# supports it (GCC 16+ with C++26 — older standards reject the
# flag outright, so CI on C++20 skips it).
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16 AND
    NOT BAKAJITEN_CXX_STANDARD LESS 26)
    add_compile_options(-freflection)
endif()

include(cmake/uniffi.cmake)
include(cmake/app.cmake)
include(cmake/install.cmake)
include(cmake/analysis.cmake)
include(cmake/tests.cmake)
