#pragma once

#include <QObject>
#include <QString>

class QLineEdit;

/// IME-like romaji → kana for the search field: a raw romaji buffer behind the
/// visible text, so ASCII letters convert as they land (`ta` →
/// `た`, pending tail shown raw). Conversion extends the romaji
/// run at the cursor: only the maximal trailing [a-z'-] run ending
/// at the cursor plus the keystroke re-converts (`あn` + `a` →
/// `あな`), so a mid-word `n` stays raw until settled. `-` lengthens
/// and `/` becomes the middle dot through the same path (both settle
/// the pending run first). While the run at the cursor is pending the
/// backspace and conversion keystrokes land (arrows, space,
/// return wait), and neither keys nor clicks move the cursor.
/// Backspace keeps default handling and deletes the last display
/// character whole (never un-converts); the buffer re-syncs from
/// the field text. ASCII letters convert through the Rust table;
/// everything else (direct kana, paste, candidate inserts) falls
/// through to the field and re-syncs the buffer.
/// Owned by DictionaryWidget, rebound per form (dev reload swaps
/// the field).
class KanaInput : public QObject {
    Q_OBJECT
public:
    explicit KanaInput(QLineEdit *field);
    /// Point at a new field after a form rebuild.
    void rebind(QLineEdit *field);

    /// Batch query text for any field content (ASCII runs convert,
    /// kana passes through). Used by onSearch, so pasted romaji
    /// searches converted too.
    static QString searchText(const QString &text);
    /// A mora is mid-conversion (live tail shown raw): the widget
    /// holds searches until it settles. Always false with
    /// conversion disabled.
    bool hasPending() const;
    /// Romaji→kana keystroke handling on/off (meaning search takes
    /// literal text). Defaults on.
    void setConversionEnabled(bool enabled);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    /// External text change (paste, candidate insert, clear):
    /// the buffer becomes the literal text.
    void onFieldChanged(const QString &text);

private:
    /// Render display(raw); cursor to the converted head, or to
    /// the end when cursorPos is negative.
    void render(int cursorPos = -1);

    QLineEdit *field = nullptr;
    QString raw;
    bool rendering = false;
    /// Romaji→kana handling (meaning search takes literal text).
    bool conversionEnabled = true;
};
