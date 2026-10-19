#pragma once

#include <QObject>

class MainWindow;
class RecognizerWidget;
class QPushButton;

/// Main-window flows fixture (one test binary, methods split by
/// area across TestMainWindow*.cpp). Shared helpers live in
/// TestMainWindow.cpp; offscreen platform, real mouse events.
class TestMainWindow : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void panelHiddenByDefault();
    void toolbarSquareIcons();
    void openButtonShowsPanel();
    void openButtonTogglesPanelClosed();
    void panelOpensUnderButton();
    void clickAwayHidesPanel();
    void panelOpenFocusesInput();
    void searchFieldClickKeepsPanel();
    void helpMenuShowsAbout();
    void helpMenuShowsAboutQt();
    void updaterVersionCompare();
    void updaterAssetMatch();
    void updaterInstallerKind();
    void updaterSwapScript();
    void updaterArchiveZip();
    void updaterArchiveTarGz();
    void updaterPromptSkip();
    void updaterProgressFormat();
    void updaterSkipPersists();
    void fadesFollowScrollability();
    void queryTimeShownAfterStroke();
    void candidateScriptColors();
    void candidateClickInsertsQuery();
    void candidateInsertsAtCursor();
    void candidateDoubleClickOpensDialog();
    void candidateSlowClicksInsertTwice();
    void kanaInputConvertsWhileTyping();
    void kanaInputSearchesConverted();
    void kanaInputModalWhilePending();
    void kanaInputPartialMoraSkipsSearch();
    void kanjiDialogOpensFromTable();
    void kanjiDialogOpensFromArticle();
    void kanjiDialogIgnoresKana();
    void kanjiDialogCaptionsAreBold();
    void kanjiDialogShowsFullDetail();
    void kanjiDialogSpanSelectsDetails();
    void kanjiDialogCaptionSelects();
    void kanjiStrokeViewPaints();
    void kanjiStrokeBadgesAvoidOverlap();
    void kanjiInfoStrokesFromData();
    void kanjiDialogWordsTab();
    void kanjiDialogSimilarStrip();
    void kanjiDialogPartsStrip();
    void kanjiDialogPartOfStrip();
    void kanjiDialogStripShowsKanjiTip();
    void kanjiDialogTogglePreservesMainArea();
    void kanjiDialogStripStatePersists();
    void kanjiDialogRetargetShowsInfo();
    void kanjiStrokePlaybackSteps();
    void kanjiStrokePlaybackPlays();
    void kanjiStrokePlaybackDrawsPartial();
    void kanjiDialogShowsCounter();
    void scaleRoundTrip();
    void toolbarFollowsFont();
    void settingsDialogReportsScale();
    void settingsDialogReadingOptions();
    void settingsDialogUpdatesOptOut();
    void settingsDialogDirtyTracking();
    void settingsDialogApplyUnDirties();
    void settingsDialogScaleRecreates();
    void settingsScalesBothFontModes();
    void fontScaleRescalesTableFont();
    void fontScaleRefitsLiveChrome();
    void fontScaleRescalesMemorizedWidths();
    void candidateScaleStaysIdempotent();
    void themeRoundTrip();
    void themeLinkStaysBlue();
    void themeStrokeBadgeRampsBlueToGreen();
    void themeWarningStaysRed();
    void iconsResolveFromTheme();
    void settingsNestShape();
    void settingsFlatFallback();
    void candidatesFollowScale();
    void dictionaryTableScrollsSmoothlyAndStretches();
    void dictionaryTableDefaultsScrollPerPixel();
    void dictionaryTypingRedirectsToSearch();
    void dictionaryRowsMaterializeOnDemand();
    void dictionaryShowsAllSenseTagsAndSkipsEmptyMeanings();
    void dictionaryHeadingDragSelectsAndCopies();
    void dictionarySearchModeTogglesSuffix();
    void dictionarySearchModeBothTogglesSubstring();
    void dictionarySearchModeNeitherIsExact();
    void dictionarySearchMeaningMode();
    void dictionarySearchScopeCombo();
    void dictionaryColumnWidthsPersist();
    void dictionaryArticleWidthPersists();
    void dictionaryResizePreservesArticleWidth();
    void dictionarySelectionKeepsTableScroll();
    void dictionaryArticleFollowsSelection();
    void dictionaryArticleSpanSelectsMeanings();
    void dictionaryArticleSpanIgnoresHeading();
    void dictionaryArticleSpanClearsBeyondFocus();
    void dictionaryKanjiHoverShowsInfo();
    void dictionaryKanjiCardMatchesMock();
    void dictionaryKanjiPartsFromData();
    void dictionaryTooltipCardAlignsToCursor();
    void dictionaryArticleTipFollowsScroll();
    void dictionaryArticleHoverPastEndSafe();
    void dictionaryKanjiCardFollowsTheme();
    void dictionaryQueryResetsScrollToTop();
    void dictionaryArticleLinkInsertsQuery();
    void dictionaryArticleRealXrefRendersAsLink();
    void dictionaryTableShowsMatchedForm();
    void dictionaryDeinflectedRowsTinted();
    void dictionaryDeinflectedShowsFormName();
    void dictionaryTableHighlightsMatch();
    void dictionaryArticleShowsTextCursor();
    void dictionaryArticleCopiesSelection();
    void dictionaryTagTooltipShowsDescription();
    void dictionaryTagTooltipSurvivesWithinRunMoves();
    void dictionaryTagTooltipSurvivesRowSweep();
    void dictionaryMeaningLayoutElidesOverflow();
    void dictionaryMeaningTagsShareBaseline();
    void dictionaryUkTagRendersRed();
    void dictionaryTagTipsPairPerTag();
    void dictionaryArticleBuildsWidgets();
    void dictionaryArticleMetaLinesBreak();
    void dictionaryArticleLinksUnderlined();
    void dictionaryArticleUkRendersRed();
    void dictionaryArticleTagTipsPairPerTag();
    void dictionaryRubySelectsKeb();
    void dictionaryHeadingSelectsFully();
    void dictionaryArticleTagTooltipShowsDescription();
    void dictionaryArticlePaintsRunInks();
    void dictionaryArticleRerendersOnPaletteChange();
    void dictionaryArticleCloseButtonHidesPane();
    void dictionaryKanaOnlyHeadingTransparent();
    void dictionaryRomajiConvertsBothSchools();
    void dictionaryRomajiBatchConvertsToKana();
    void dictionaryRomajiDisplayShowsPendingTail();
    void dictionaryFuriganaSegments();
    void dictionaryReadingFollowsSetting();
    void dictionaryTableWheelStepsImmediately();
    void dictionaryHoverHighlightsRow();
    void dictionarySearchLiveAsYouType();
};

/// Shown top-level window (null when never exposed).
MainWindow *openWindow();
/// Handwriting panel of a window.
RecognizerWidget *panelOf(MainWindow *window);
/// Panel open button of a window.
QPushButton *openButtonOf(MainWindow *window);
