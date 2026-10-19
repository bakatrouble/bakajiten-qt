#pragma once

#include <QStyledItemDelegate>

/// Results-cell styling: heading bold and larger, reading dimmed,
/// overflowing content elided (single line — rows stay uniform, so
/// any row count scrolls without measuring). Fonts derive from the
/// view (UI scale applies automatically); colors come from the
/// palette (theme-independent). Hover washes unselected rows (soft
/// highlight, never the solid selection fill — selected rows keep
/// theirs). Heading/reading text is directly
/// selectable (I-beam, drag to select, Ctrl+C — see `DictionaryTable`);
/// the meaning cell stays display-only.
class DictionaryTableDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit DictionaryTableDelegate(QObject *parent = nullptr);

    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

private:
    /// Meaning cell (column 2): tag prefix plus elided body.
    void paintMeaning(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index) const;
    /// Style background plus the manual hover wash (see `washFor`):
    /// the shared base for every custom cell, so text never has to
    /// fight the style for its backdrop.
    void paintBackground(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const;
};
