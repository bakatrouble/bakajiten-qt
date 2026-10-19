#pragma once

#include <QWidget>

#include <QLoggingCategory>
#include <QString>
#include <QStringList>

/// Dev-only runtime .ui loading (QUiLoader). This header is backend-
/// neutral (no UiTools includes); `UiDev.cpp` — compiled only with
/// `-DDEV_UILOADER=ON` — implements the loader.
///
/// Custom .ui root classes (`RecognizerWidget`, `DictionaryWidget`) map
/// to plain QWidgets; only `CanvasWidget` and `CandidateList` are
/// constructed for real (they own behavior). Layouts, properties, and
/// object names all still apply. The caller (`UiForm.h`) fills the
/// uic `Ui::` struct from the loaded tree by reflection, so .ui files
/// double as both compile-time and runtime sources unchanged.

/// Directory the .ui files are read from: `$UI_DIR` wins, else
/// the `UI_DIR` compile definition (the source `src/` dir).
QString uiDir();

/// Load `uiFileName` (e.g. `"recognizer/RecognizerWidget.ui"`) from
/// [`uiDir`] as a child of `parent`. Returns null (logged) when
/// the file is missing or unparsable.
QWidget *loadUiForm(const QString &uiFileName, QWidget *parent);

/// Every .ui file under [`uiDir`], recursively — the watch list.
/// New forms are picked up on restart without touching any list.
/// (Files created while running need a restart: nothing would
/// consume them yet, since each form is hosted by its own widget.)
QStringList uiFormFiles();

Q_DECLARE_LOGGING_CATEGORY(uiLoaderLog);
