#pragma once

#include <QIcon>
#include <QSize>
#include <QString>

class QAbstractButton;
class QWidget;

/// Square toolbar icons from `resources/icons.qrc`: 24px artwork in
/// the live button-text color (re-tinted per theme — Qt's SVG
/// renderer can't be trusted with `currentColor`, so the tint lands
/// before render). `apply` records the name on the button;
/// `refresh` re-tints everything under a widget on theme switches.
class ButtonIcon {
public:
    /// Rendered icon side (the 40px buttons center it).
    static constexpr int sidePx = 30;

    /// Set a button's icon from `:/icons/<name>.svg` (clears its
    /// text; tooltips stay the label).
    static void apply(QAbstractButton *button, const QString &name);
    /// Re-tint all applied icons under `root` (theme switches).
    static void refresh(QWidget *root);
    /// One themed icon (for tests and one-offs).
    static QIcon icon(const QString &name);
};
