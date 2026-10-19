#pragma once

#include <QObject>
#include <QString>

class QLineEdit;

/// IME-like romaji → kana for the search field: a raw romaji buffer behind the
/// visible text, so ASCII letters convert as they land (`ta` →
/// `た`, pending tail shown raw). While a mora is pending the
/// field is modal: only backspace and conversion keystrokes land
/// (arrows, space, return wait), and neither keys nor clicks move
/// the cursor. Backspace keeps default handling and deletes the
/// last display character whole (never un-converts); the buffer
/// re-syncs from the field text. ASCII letters at the end of the
/// field convert through the Rust table; everything else (direct
/// kana, paste, mid-text edits, candidate inserts) falls through
/// to the field and re-syncs the buffer.
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
    /// Render display(raw), cursor to end.
    void render();
    bool atEnd() const;

    QLineEdit *field = nullptr;
    QString raw;
    bool rendering = false;
    /// Romaji→kana handling (meaning search takes literal text).
    bool conversionEnabled = true;
};
