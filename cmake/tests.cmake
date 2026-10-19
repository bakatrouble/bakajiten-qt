# Qt unit tests (offscreen platform, no pixels, no mouse).
# Format-checked like app sources, but not clang-tidied (QTest
# boilerplate trips style checks not worth chasing).
option(BUILD_QT_TESTS "Build Qt unit tests" ON)
if(BUILD_QT_TESTS)
    find_package(Qt6 COMPONENTS Test REQUIRED)
    enable_testing()
    add_executable(test_mainwindow
        tests/TestMainWindow.h
        tests/TestDataManifest.h
        tests/TestDataManifest.cpp
        tests/TestGzip.h
        tests/TestGzip.cpp
        tests/TestPanel.h
        tests/TestPanel.cpp
        tests/TestPanelSize.h
        tests/TestPanelSize.cpp
        tests/TestMainWindow.cpp
        tests/TestMainWindowPanel.cpp
        tests/TestMainWindowUpdate.cpp
        tests/TestMainWindowCandidates.cpp
        tests/TestMainWindowKanaInput.cpp
        tests/TestMainWindowKanjiDialog.cpp
        tests/TestMainWindowTableView.cpp
        tests/TestMainWindowTableSearch.cpp
        tests/TestMainWindowTableMatch.cpp
        tests/TestMainWindowTableTooltips.cpp
        tests/TestMainWindowTablePaint.cpp
        tests/TestMainWindowArticle.cpp
        tests/TestMainWindowArticleHover.cpp
        tests/TestMainWindowArticleBuild.cpp
        tests/TestMainWindowAppearance.cpp
        tests/TestMainWindowSettings.cpp
        tests/TestMainWindowScale.cpp
        tests/TestMainWindowPersist.cpp
        tests/TestMainWindowRomaji.cpp
        src/ui/mainwindow/MainWindow.cpp
        src/ui/mainwindow/MainWindow.ui
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
        src/ui/dictionary/DictionaryWidget.ui
        src/core/dictionary/DictionaryModel.cpp
        src/core/dictionary/DictionaryModelDisplay.cpp
        src/util/Furigana.cpp
        src/util/Archive.cpp
        src/widgets/input/KanaInput.cpp
        src/core/dictionary/MeaningBuilder.cpp
        src/widgets/table/DictionaryTableColors.cpp
        src/widgets/table/DictionaryTableDelegate.cpp
        src/widgets/tooltip/KanjiTooltip.cpp
        src/widgets/tooltip/KanjiTooltip.ui
        src/widgets/tooltip/KanjiHover.cpp
        src/widgets/tooltip/KanjiHoverHit.cpp
        src/widgets/icons/ButtonIcon.cpp
        resources/icons.qrc
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
        src/ui/settings/SettingsDialog.cpp
        src/ui/recognizer/RecognizerWidget.cpp
        src/ui/recognizer/RecognizerWidgetChrome.cpp
        src/ui/recognizer/RecognizerWidget.ui
        src/widgets/list/CandidateListDelegate.cpp
        src/widgets/list/CandidateList.cpp
        src/ui/recognizer/Candidates.cpp
        src/ui/recognizer/Strokes.cpp
        src/ui/recognizer/Drag.cpp
        src/widgets/edgefade/EdgeFade.cpp
        src/widgets/tooltip/Tooltip.cpp
        src/widgets/canvas/CanvasWidget.cpp
    )
    target_include_directories(test_mainwindow PRIVATE src)
    target_link_libraries(test_mainwindow PRIVATE appsettings dict recognizer uniffi_recognizer uniffi_dict Qt6::Widgets Qt6::Svg Qt6::Network Qt6::Test)
    target_compile_definitions(test_mainwindow PRIVATE "BAKAJITEN_UPDATE_ARTIFACT=\"appimage\"")
    add_test(NAME data_manifest COMMAND test_mainwindow data)
    set_tests_properties(data_manifest PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME panel_size_math COMMAND test_mainwindow panelsize)
    set_tests_properties(panel_size_math PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME panel_square_canvas COMMAND test_mainwindow panel)
    set_tests_properties(panel_square_canvas PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME main_window_flows COMMAND test_mainwindow mainwindow)
    set_tests_properties(main_window_flows PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
