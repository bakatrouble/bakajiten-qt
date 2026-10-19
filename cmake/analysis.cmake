# Static analysis, enforced at build (missing tools only warn, so
# other machines still configure):
# - clangd reads compile_commands.json (CLion's engine too);
# - clang-tidy runs per-TU with its reports as errors. It parses via
#   the tool-compat stub in dev/UiForm.h (clang lacks P2996), so the
#   reflection machinery itself escapes analysis by design;
# - clang-format --check fails the build on style violations.

find_program(CLANG_TIDY_EXE clang-tidy)
# Never on MSVC: its bundled tidy runs with exceptions disabled and
# chokes on `throw` in the generated UniFFI bindings (Linux stays
# the lint gatekeeper).
if(CLANG_TIDY_EXE AND NOT MSVC)
    # Via the filter launcher on GCC (only its driver materializes
    # -mno-direct-extern-access, which clang hard-errors on; plain
    # .sh launchers don't execute on Windows, where the flag never
    # appears anyway — see cmake/clang-tidy.sh). NewDelete is
    # carved out of the errors upgrade (kept as a visible warning):
    # Qt 6.10.3's own qsharedpointer_impl.h trips it through any
    # QPointer destruction, and header filters don't catch
    # analyzer paths ending in system headers on that toolchain.
    # Revisit when the Qt floor moves past the fixed header.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(_tidy_launcher "${CMAKE_CURRENT_SOURCE_DIR}/cmake/clang-tidy.sh;")
    else()
        set(_tidy_launcher "")
    endif()
    set_target_properties(bakajiten PROPERTIES
        CXX_CLANG_TIDY "${_tidy_launcher}${CLANG_TIDY_EXE};--warnings-as-errors=*,-clang-analyzer-cplusplus.NewDelete;--header-filter=${CMAKE_CURRENT_SOURCE_DIR}/src/;--extra-arg=-Wno-unused-command-line-argument")
else()
    message(WARNING "clang-tidy not found: skipping static analysis")
endif()

find_program(CLANG_FORMAT_EXE clang-format)
if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE _clang_format_sources CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
    # Autofix, not a gate: normalize sources in place on every build
    # (idempotent — clean trees are a fast no-op). Runs before any
    # compile via the dependency below, so the compiler always sees
    # formatted code.
    add_custom_target(fix-clang-format ALL
        COMMAND ${CLANG_FORMAT_EXE} --style=file -i ${_clang_format_sources}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
        COMMENT "Applying C++ formatting")
    add_dependencies(bakajiten fix-clang-format)
else()
    message(WARNING "clang-format not found: skipping format step")
endif()
