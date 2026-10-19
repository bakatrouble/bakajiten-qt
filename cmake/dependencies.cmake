# Qt interface dependencies: Qt6 modules, YAML settings storage,
# the Rust recognizer + dictionary (built via Corrosion), nothing else.
# Defines the `librecognizer`/`libdict` CMake targets used below
# (static for linking, shared for binding generation).
find_package(Qt6 COMPONENTS Core Gui Widgets Svg Network REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(ZLIB REQUIRED)
# yaml-cpp < 0.7 exports a plain `yaml-cpp` target without namespace.
if(NOT TARGET yaml-cpp::yaml-cpp AND TARGET yaml-cpp)
    add_library(yaml-cpp::yaml-cpp ALIAS yaml-cpp)
endif()

find_package(Corrosion 0.5.2 QUIET)
if(NOT Corrosion_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        Corrosion
        GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
        GIT_TAG v0.5.2
    )
    FetchContent_MakeAvailable(Corrosion)
endif()

# Provides the UniFFI scaffolding symbols the generated bindings
# call into; the recognizer itself is header-driven from the
# build-tree bindings (see uniffi.cmake).
corrosion_import_crate(
    MANIFEST_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../Cargo.toml
    CRATES librecognizer libdict
)

# Corrosion hardcodes the CLT SDK usr/lib as an interface link
# directory on its macOS imported targets, and those TBDs are
# unparsable by the newer linker (tapi "unknown architecture").
# -isysroot already covers system libs, so drop the inherited
# paths (guarded: exact imported-target names vary by Corrosion
# version).
if(APPLE)
    foreach(_corrosion_tgt IN ITEMS libdict-static librecognizer-static dict-static
            recognizer-static)
        if(TARGET ${_corrosion_tgt})
            set_property(TARGET ${_corrosion_tgt} PROPERTY INTERFACE_LINK_DIRECTORIES "")
        endif()
    endforeach()
    unset(_corrosion_tgt)
endif()
