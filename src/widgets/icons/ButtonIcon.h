#pragma once

#include <QColor>
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
/// The checked-On state renders in a contrasting ink sampled
/// against the style's real checked-button fill (white under
/// Kvantum, dark under Fusion) instead of the plain button text.
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
    /// Ink readable on `background`: `base` untouched when it
    /// already stands out, lightness-flipped otherwise.
    static QColor contrastingInk(const QColor &base, const QColor &background);
    /// On-state glyph ink for a checkable button: when the style
    /// paints a distinct checked fill, the ink contrasts with it;
    /// when checked and unchecked fills match (the style gives no
    /// background cue, e.g. windows11 flat buttons), the glyph
    /// itself carries the state in the accent color. Pure logic,
    /// unit-tested.
    static QColor checkedInk(const QColor &ink, const QColor &checkedFill,
                             const QColor &uncheckedFill, const QColor &highlight,
                             const QColor &window);
};
