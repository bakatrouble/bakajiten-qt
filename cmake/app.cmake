# The application itself: C++ owns the windows, Rust (see
# dependencies.cmake) owns the matching.
# AppSettings: persisted appearance, compiled once and shared by
# the app and the widget-level tests.
add_library(appsettings STATIC
    src/core/settings/Settings.cpp
    src/core/settings/YamlFormat.cpp
    src/core/settings/MemorizedState.cpp
    src/core/data/DataPaths.cpp
    src/core/data/DataManifest.cpp
    src/core/data/Gzip.cpp
)
target_include_directories(appsettings PUBLIC src)
target_link_libraries(appsettings PUBLIC Qt6::Widgets yaml-cpp::yaml-cpp ZLIB::ZLIB)

add_executable(bakajiten
    src/main.cpp
    src/ui/mainwindow/MainWindow.cpp
    src/ui/mainwindow/AutoHide.cpp
    src/widgets/text/ArticleView.cpp
    src/widgets/text/ArticleViewHover.cpp
    src/widgets/text/ArticleRuns.cpp
    src/widgets/text/RunLabel.cpp
    src/widgets/text/RunLabelLayout.cpp
    src/widgets/text/RubyLabel.cpp
    src/widgets/text/RubyLabelLayout.cpp
    src/ui/dictionary/DictionaryWidget.cpp
    src/ui/dictionary/DictionaryWidgetArticle.cpp
    src/ui/dictionary/DictionaryWidgetKanji.cpp
    src/ui/dictionary/DictionaryWidgetSearch.cpp
    src/ui/settings/SettingsDialog.cpp
    src/ui/recognizer/RecognizerWidget.cpp
    src/ui/recognizer/RecognizerWidgetChrome.cpp
    src/widgets/list/CandidateListDelegate.cpp
    src/widgets/list/CandidateList.cpp
    src/ui/recognizer/Candidates.cpp
    src/ui/recognizer/Strokes.cpp
    src/ui/recognizer/Drag.cpp
    src/widgets/edgefade/EdgeFade.cpp
        src/widgets/tooltip/Tooltip.cpp
    src/widgets/canvas/CanvasWidget.cpp
    src/core/dictionary/DictionaryModel.cpp
    src/core/dictionary/DictionaryModelDisplay.cpp
    src/util/Furigana.cpp
    src/util/Archive.cpp
    src/widgets/input/KanaInput.cpp
    src/core/dictionary/MeaningBuilder.cpp
    src/widgets/table/DictionaryTableColors.cpp
    src/widgets/table/DictionaryTableDelegate.cpp
    src/widgets/table/DictionaryTableDelegate.h
    src/widgets/tooltip/KanjiTooltip.cpp
    src/widgets/tooltip/KanjiTooltip.ui
    src/widgets/tooltip/KanjiHover.cpp
    src/widgets/tooltip/KanjiHoverHit.cpp
    src/ui/kanji/KanjiDialog.cpp
    src/ui/update/UpdateChecker.cpp
    src/ui/update/UpdateInstaller.cpp
    src/ui/update/UpdatePrompt.cpp
    src/ui/update/UpdateProgress.cpp
    src/ui/data/DataChecker.cpp
    src/ui/data/DataDownloader.cpp
    src/ui/data/DataDialog.cpp
    src/ui/kanji/KanjiDialog.ui
    src/ui/kanji/KanjiStrokeView.cpp
    src/util/Romaji.cpp
    src/util/RomajiDisplay.cpp
    src/widgets/table/DictionaryTable.cpp
    src/widgets/table/DictionaryTableHover.cpp
    src/widgets/table/DictionaryTableScroll.cpp
    src/widgets/table/DictionaryTableSelect.cpp
    src/widgets/table/DictionaryTableTags.cpp
    src/widgets/icons/ButtonIcon.cpp
    resources/icons.qrc
)
target_include_directories(bakajiten PRIVATE src)
# Direct: the shell uses Widgets/Gui throughout (main.cpp,
# mainwindow, canvas); this used to arrive transitively via the
# CXX-Qt crate target. Link the top Rust crate only (`libdict`
# bundles `librecognizer` into its staticlib — linking both
# duplicates the UniFFI symbols).
target_link_libraries(bakajiten PRIVATE Qt6::Widgets Qt6::Svg Qt6::Network appsettings dict recognizer uniffi_recognizer uniffi_dict)
# Optional embedded CJK fallback (Noto Sans JP): downloaded once at
# configure time into the build tree and compiled into the binary via
# a generated .qrc. Off everywhere except the Windows cross build
# (see build-win64.sh): other platforms use system fonts.
option(BAKAJITEN_BUNDLE_CJK_FONT "Embed Noto Sans JP fallback font" OFF)
if(BAKAJITEN_BUNDLE_CJK_FONT)
    # Google's variable font defaults to Thin (wght=100), which
    # renderers without axis selection (Wine) use as-is — thin
    # glyphs. Instantiate a static Regular and normalize its names
    # (instancer keeps the "Thin" family), then bundle that.
    set(_cjk_var "${CMAKE_CURRENT_BINARY_DIR}/fonts/NotoSansJP-VF.ttf")
    set(_cjk_ttf "${CMAKE_CURRENT_BINARY_DIR}/fonts/NotoSansJP.ttf")
    if(NOT EXISTS "${_cjk_var}")
        message(STATUS "Downloading Noto Sans JP fallback font")
        file(DOWNLOAD
            "https://raw.githubusercontent.com/google/fonts/main/ofl/notosansjp/NotoSansJP%5Bwght%5D.ttf"
            "${_cjk_var}"
            EXPECTED_HASH SHA256=c2f3b4d463500a2ddcd3849cded1fceeb9fd6d1c32e6cbecd568453ba50fc68f)
    endif()
    find_package(Python3 COMPONENTS Interpreter)
    if(NOT Python3_Interpreter_FOUND)
        message(FATAL_ERROR "BAKAJITEN_BUNDLE_CJK_FONT needs a Python 3 interpreter (fontTools instancer)")
    endif()
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -c "import fontTools"
        RESULT_VARIABLE _cjk_has_fonttools
        OUTPUT_QUIET ERROR_QUIET)
    if(_cjk_has_fonttools)
        message(STATUS "Installing fontTools into the build tree")
        execute_process(
            COMMAND "${Python3_EXECUTABLE}" -m pip install --quiet --target
                "${CMAKE_CURRENT_BINARY_DIR}/fonts/pylibs" fonttools
            RESULT_VARIABLE _cjk_pip_result
            ERROR_VARIABLE _cjk_pip_error)
        if(_cjk_pip_result)
            message(FATAL_ERROR "pip install fonttools failed: ${_cjk_pip_error}")
        endif()
    endif()
    set(_cjk_env "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/fonts/pylibs")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "${_cjk_env}" "${Python3_EXECUTABLE}" -m
            fontTools.varLib.instancer "${_cjk_var}" wght=400 -o "${_cjk_ttf}"
        RESULT_VARIABLE _cjk_instance_result)
    if(_cjk_instance_result)
        message(FATAL_ERROR "Instantiating the static Noto Sans JP Regular failed")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "${_cjk_env}" "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/rename-cjk-font.py" "${_cjk_ttf}"
        RESULT_VARIABLE _cjk_rename_result)
    if(_cjk_rename_result)
        message(FATAL_ERROR "Normalizing the Noto Sans JP names failed")
    endif()
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/fonts/fonts.qrc"
        "<RCC>\n    <qresource prefix=\"/fonts\">\n        <file>NotoSansJP.ttf</file>\n    </qresource>\n</RCC>\n")
    target_sources(bakajiten PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/fonts/fonts.qrc")
endif()
# Update artifact kind: which release asset counts as this
# build's update (win64 zip, AppImage, linux-static tarball,
# macos-arm64/macos-x64 zips). Empty disables update checks.
set(BAKAJITEN_UPDATE_ARTIFACT "" CACHE STRING "Update artifact kind")
target_compile_definitions(bakajiten PRIVATE "BAKAJITEN_UPDATE_ARTIFACT=\"${BAKAJITEN_UPDATE_ARTIFACT}\"")
# Static Qt links no platform plugin DLL: import the platform
# integration statically. Only for static Qt builds (shared builds
# resolve plugins from their DLLs at runtime — and their plugin
# targets carry no static instance symbol to link).
get_target_property(_qtCoreType Qt6::Core TYPE)
if(_qtCoreType STREQUAL "STATIC_LIBRARY")
    if(TARGET Qt6::QWindowsIntegrationPlugin)
        target_link_libraries(bakajiten PRIVATE Qt6::QWindowsIntegrationPlugin)
    endif()
    if(TARGET Qt6::QXcbIntegrationPlugin)
        target_link_libraries(bakajiten PRIVATE Qt6::QXcbIntegrationPlugin)
    endif()
    if(TARGET Qt6::QWaylandIntegrationPlugin)
        target_link_libraries(bakajiten PRIVATE Qt6::QWaylandIntegrationPlugin)
    endif()
    # System integration (dark mode, fonts, icon theme, portal file
    # dialogs) on freedesktop hosts; harmless where no portal runs.
    if(TARGET Qt6::QXdgDesktopPortalThemePlugin)
        target_link_libraries(bakajiten PRIVATE Qt6::QXdgDesktopPortalThemePlugin)
    endif()
    # Static OpenSSL backend (linux-static Qt builds `-openssl-linked`;
    # Schannel/SecureTransport builds have no such plugin target).
    if(TARGET Qt6::QTlsBackendOpenSSLPlugin)
        target_link_libraries(bakajiten PRIVATE Qt6::QTlsBackendOpenSSLPlugin)
        target_compile_definitions(bakajiten PRIVATE BAKAJITEN_STATIC_TLS)
    endif()
    target_compile_definitions(bakajiten PRIVATE BAKAJITEN_STATIC_QT)
endif()

if(DEV_UILOADER)
    find_package(Qt6 COMPONENTS UiTools REQUIRED)
    target_sources(bakajiten PRIVATE src/dev/UiDev.cpp)
    target_compile_definitions(bakajiten PRIVATE
        DEV_UILOADER
        UI_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(bakajiten PRIVATE Qt6::UiTools)
endif()
