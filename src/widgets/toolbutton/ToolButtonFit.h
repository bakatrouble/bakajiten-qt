#pragma once

#include <QFontMetrics>
#include <QPushButton>
#include <QWidget>

/// Square toolbar buttons fitting the interface font: the side fits
/// the button's text at its live font (never below the 40px design
/// minimum), so scaled text is never clipped by the fixed box — on
/// a fresh window and on live font changes alike. The floor stays an
/// absolute 40px touch target (never scaled: scaling it overshoots
/// the text — 60px boxes for 45px of text at 150%). Measured from
/// the text itself: the style's 80px push-button minimum would break
/// the square design, so it stays out (a 12px pad keeps the text
/// clear of the focus frame).
inline int toolButtonSide(const QPushButton *button, int basePx) {
    const QFontMetrics metrics(button->font());
    const int textSide = qMax(metrics.horizontalAdvance(button->text()), metrics.height()) + 12;
    return qMax(basePx, textSide);
}

inline void fitToolButtons(QWidget *scope, int basePx) {
    for (auto *button : scope->findChildren<QPushButton *>()) {
        const int side = toolButtonSide(button, basePx);
        button->setFixedSize(side, side);
    }
}

/// Wide toolbar button for multi-char labels (the search target
/// toggles): width fits the text, height matches the given square
/// side — extended width is fine, a taller row is not.
inline void fitWideToolButton(QPushButton *button, int sidePx) {
    const QFontMetrics metrics(button->font());
    button->setFixedSize(qMax(sidePx, metrics.horizontalAdvance(button->text()) + 12), sidePx);
}
