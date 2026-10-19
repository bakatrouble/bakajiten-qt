# UniFFI C++ bindings: generated at build time from the just-built
# `librecognizer`/`libdict` cdylibs (always in sync with `ffi.rs` —
# the generated code embeds checksums and throws on drift, so a
# stale check-in could never pass silently). Needs the external
# generator once per machine (pinned to the workspace's UniFFI):
#
#   cargo install uniffi-bindgen-cpp \
#     --git https://github.com/NordSecurity/uniffi-bindgen-cpp \
#     --tag v0.10.0+v0.31.0
find_program(UNIFFI_BINDGEN_CPP uniffi-bindgen-cpp
    DOC "UniFFI C++ binding generator (see install command above)")
if(NOT UNIFFI_BINDGEN_CPP)
    message(FATAL_ERROR "uniffi-bindgen-cpp not found on PATH")
endif()

set(UNIFFI_CPP_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/bindings-cpp)
# Freshness: a bare target-level dep would never re-fire once the
# outputs exist, and $<TARGET_FILE:> degrades to a bare filename in
# DEPENDS (observed: Make watched `librecognizer.so` with no path).
# Depend on the Rust sources + manifests instead — any edit
# or dependency bump rebuilds the .so, so these cover it exactly.
file(GLOB_RECURSE UNIFFI_RUST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/librecognizer/src/*.rs
)
file(GLOB_RECURSE UNIFFI_DICT_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/libdict/src/*.rs
)
add_custom_command(
    OUTPUT
        ${UNIFFI_CPP_OUT_DIR}/recognizer.hpp
        ${UNIFFI_CPP_OUT_DIR}/recognizer.cpp
        ${UNIFFI_CPP_OUT_DIR}/recognizer_scaffolding.hpp
    COMMAND ${UNIFFI_BINDGEN_CPP}
        --library $<TARGET_FILE:recognizer-shared>
        --out-dir ${UNIFFI_CPP_OUT_DIR}
    DEPENDS recognizer-shared ${UNIFFI_RUST_SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Cargo.toml
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Cargo.lock
        ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/librecognizer/Cargo.toml
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..
    COMMENT "Generating UniFFI C++ bindings"
    VERBATIM
)

# The generated TU lives in its own target on purpose: exempt from
# clang-tidy by construction (the tidy property in analysis.cmake
# is set per-target on `bakajiten` only) and from the clang-format
# autofix (whose GLOB covers `src/` and `tests/`, never the build
# tree).
add_library(uniffi_recognizer STATIC
    ${UNIFFI_CPP_OUT_DIR}/recognizer.cpp
    src/core/recognizer/Recognizer.cpp
    src/core/recognizer/RecognizerWorker.cpp
)
target_include_directories(uniffi_recognizer PUBLIC ${UNIFFI_CPP_OUT_DIR} src)
target_link_libraries(uniffi_recognizer PUBLIC Qt6::Core)

add_custom_command(
    OUTPUT
        ${UNIFFI_CPP_OUT_DIR}/dict.hpp
        ${UNIFFI_CPP_OUT_DIR}/dict.cpp
        ${UNIFFI_CPP_OUT_DIR}/dict_scaffolding.hpp
    COMMAND ${UNIFFI_BINDGEN_CPP}
        --library $<TARGET_FILE:dict-shared>
        --out-dir ${UNIFFI_CPP_OUT_DIR}
    DEPENDS dict-shared ${UNIFFI_DICT_SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Cargo.toml
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Cargo.lock
        ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/libdict/Cargo.toml
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../..
    COMMENT "Generating UniFFI C++ dictionary bindings"
    VERBATIM
)

# Dictionary bindings TU (same exemptions as above). Not linked into
# `bakajiten` yet — the dictionary UI that consumes it is follow-up
# work; this target keeps generation itself under `ctest` cover.
add_library(uniffi_dict STATIC
    ${UNIFFI_CPP_OUT_DIR}/dict.cpp
    src/core/dictionary/DictStore.cpp
    src/core/dictionary/DictWorker.cpp
    src/core/dictionary/KanjiInfo.cpp
)
# Ordering: RecognizerWorker.cpp includes DictStore.h (dict.hpp),
# so the recognizer TU must wait for the dict bindings to generate.
# Without this, fresh build dirs race generation against compilation.
add_dependencies(uniffi_recognizer uniffi_dict)
target_include_directories(uniffi_dict PUBLIC ${UNIFFI_CPP_OUT_DIR} src)
target_link_libraries(uniffi_dict PUBLIC Qt6::Core)
