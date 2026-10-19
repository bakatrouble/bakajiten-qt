#!/usr/bin/env bash
# clang-tidy launcher: drop GCC-only driver flags that clang hard-errors
# on (this toolchain's GCC materializes -mno-direct-extern-access on
# every command line), then exec the real binary. Usage from CMake:
#   <this-script> <real-clang-tidy> [tidy args...] <source> -- <flags...>
set -u
tidy=$1
shift
args=()
for arg in "$@"; do
    if [[ $arg != -mno-direct-extern-access ]]; then
        args+=("$arg")
    fi
done
exec "$tidy" "${args[@]}"
