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

bool KanaInput::hasPending() const {
    if (!conversionEnabled || field == nullptr) {
        return false;
    }
    // Pending lives at the cursor, not at the end: the maximal
    // trailing [a-z'-] run ending at the cursor is the live mora
    // (`あnあ` keeps its `n` raw so `a` can settle it to な).
    const QString text = field->text();
    const int pos = field->cursorPosition();
    int start = pos;
    while (start > 0) {
        const QChar c = text.at(start - 1);
        if (c.unicode() < 128 && (c.isLetter() || c == u'-' || c == u'\'')) {
            --start;
        } else {
            break;
        }
    }
    if (start >= pos) {
        return false;
    }
    return dict::romaji_has_pending(text.mid(start, pos - start).toStdString());
}

void KanaInput::setConversionEnabled(bool enabled) { conversionEnabled = enabled; }

void KanaInput::render(int cursorPos) {
    // The buffer already holds display text (the keystroke path
    // converts the run before storing), so set it directly — no
    // second display() pass, which would commit a mid-word `n`
    // followed by kana.
    rendering = true;
    field->setText(raw);
    if (cursorPos < 0 || cursorPos > field->text().size()) {
        cursorPos = static_cast<int>(field->text().size());
    }
    field->setCursorPosition(cursorPos);
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
    const QString pressed = key->text();
    const bool ascii = pressed.size() == 1 && pressed.at(0).unicode() < 128;
    // Letters and `-` extend the romaji run; `/` converts standalone to
    // the middle dot (same run-extension path: the run scan never
    // includes `/`, so it settles the pending run first, like zkanji).
    const bool convertible =
        ascii && (pressed.at(0).isLetter() || pressed.at(0) == u'-' || pressed.at(0) == u'/');
    if (!hasPending() && field->hasSelectedText() &&
        (convertible || key->key() == Qt::Key_Backspace)) {
        // Selection replace: drop the selection first (the buffer
        // re-syncs from the field text); typing then converts at
        // the cursor, backspace is done already.
        field->del();
        if (key->key() == Qt::Key_Backspace) {
            return true;
        }
    }
    if (key->key() == Qt::Key_Backspace) {
        // Default handling: backspace deletes the last display
        // character whole — never un-converts kana back to romaji
        // — and the buffer re-syncs from the field text below.
        return QObject::eventFilter(watched, event);
    }
    if (convertible) {
        // Run extension at the cursor: the maximal trailing
        // [a-z'-] run ending at the cursor plus the keystroke
        // converts as a unit (`あn` + `a` → `あな`), so a mid-word
        // `n` stays raw until the next keystroke settles it. The
        // converter passes kana through, and display() on the pure
        // run handles nn-commit and lookahead; only the run is
        // replaced, the tail after the cursor never moves.
        const QString typed = pressed.toLower();
        const int pos = field->cursorPosition();
        const QString text = field->text();
        int start = pos;
        while (start > 0) {
            const QChar c = text.at(start - 1);
            if (c.unicode() < 128 && (c.isLetter() || c == u'-' || c == u'\'')) {
                --start;
            } else {
                break;
            }
        }
        const QString run = text.mid(start, pos - start) + typed;
        const QString conv = QString::fromStdString(dict::romaji_display(run.toStdString()));
        raw = text.left(start) + conv + text.mid(pos);
        render(start + static_cast<int>(conv.size()));
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
