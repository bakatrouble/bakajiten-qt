# App cross toolchain: Fedora MinGW targeting 64-bit Windows.
# Package mode is BOTH (not ONLY): Qt and yaml-cpp live outside the
# MinGW sysroot (/opt/*), and ONLY re-roots them under it, which
# breaks their find_package calls. Libraries/includes stay ONLY so
# the Windows target never links host .so files.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32/sys-root/mingw)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
set(Rust_CARGO_TARGET x86_64-pc-windows-gnu CACHE STRING "")
