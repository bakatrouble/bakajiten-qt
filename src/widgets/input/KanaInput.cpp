#include "widgets/input/KanaInput.h"

#include "dict.hpp"

#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QObject>
#include <Qt>

KanaInput::KanaInput(QLineEdit *field) { rebind(field); }

void KanaInput::rebind(QLineEdit *newField) {
    if (field != nullptr) {
        field->removeEventFilter(this);
        disconnect(field, &QLineEdit::textChanged, this, &KanaInput::onFieldChanged);
    }
    field = newField;
    raw = field->text();
    field->installEventFilter(this);
    connect(field, &QLineEdit::textChanged, this, &KanaInput::onFieldChanged);
}

QString KanaInput::searchText(const QString &text) {
    return QString::fromStdString(dict::romaji_to_kana(text.toStdString()));
}

bool KanaInput::atEnd() const {
    return field->cursorPosition() == field->text().size() && !field->hasSelectedText();
}

bool KanaInput::hasPending() const {
    return conversionEnabled && dict::romaji_has_pending(raw.toStdString());
}

void KanaInput::setConversionEnabled(bool enabled) { conversionEnabled = enabled; }

void KanaInput::render() {
    rendering = true;
    field->setText(QString::fromStdString(dict::romaji_display(raw.toStdString())));
    field->setCursorPosition(static_cast<int>(field->text().size()));
    rendering = false;
}

bool KanaInput::eventFilter(QObject *watched, QEvent *event) {
    if (watched != field) {
        return QObject::eventFilter(watched, event);
    }
    if (!conversionEnabled) {
        // Meaning search: literal text, no conversion, no modal.
        return QObject::eventFilter(watched, event);
    }
    if (event->type() == QEvent::MouseButtonPress) {
        // Composition modal: clicks must not move the cursor (or
        // start selections) while a mora is pending.
        if (hasPending()) {
            return true;
        }
        return QObject::eventFilter(watched, event);
    }
    if (event->type() != QEvent::KeyPress) {
        return QObject::eventFilter(watched, event);
    }
    const auto *key = static_cast<const QKeyEvent *>(event);
    if ((key->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) != 0) {
        return QObject::eventFilter(watched, event);
    }
    // Cold buffer (startup default query, cleared field): the raw
    // buffer starts as the literal text.
    if (raw.isEmpty()) {
        raw = field->text();
    }
    if (key->key() == Qt::Key_Backspace) {
        // Default handling: backspace deletes the last display
        // character whole — never un-converts kana back to romaji
        // — and the buffer re-syncs from the field text below.
        return QObject::eventFilter(watched, event);
    }
    const QString pressed = key->text();
    const bool ascii = pressed.size() == 1 && pressed.at(0).unicode() < 128;
    const bool convertible = ascii && (pressed.at(0).isLetter() || pressed.at(0) == u'-');
    if (convertible && atEnd()) {
        raw += pressed.toLower();
        render();
        return true;
    }
    // Mid-composition the field is modal: arrows, space, return
    // and friends wait for the mora to settle (or backspace).
    // Settled, everything non-converting keeps default handling.
    return hasPending() ? true : QObject::eventFilter(watched, event);
}

void KanaInput::onFieldChanged(const QString &text) {
    if (!rendering) {
        raw = text;
    }
}
