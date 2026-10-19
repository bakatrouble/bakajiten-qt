#include "widgets/icons/ButtonIcon.h"

#include <QAbstractButton>
#include <QApplication>
#include <QFile>
#include <QIconEngine>
#include <QPainter>
#include <QPalette>
#include <QScreen>
#include <QSvgRenderer>
#include <QWidget>

namespace {
/// SVG icon engine tinting `currentColor` artwork at render time
/// (normal + dimmed-disabled inks ride along, so no per-mode
/// assets). Distinct `key()` per file: sharing one would alias
/// every icon to the same cache entry.
class SvgEngine : public QIconEngine {
public:
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    SvgEngine(QString name, QByteArray svg, QColor ink, QColor dimInk)
        : name_(std::move(name)), svg_(std::move(svg)), ink_(ink), dimInk_(dimInk) {}

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode,
               QIcon::State state) override {
        Q_UNUSED(state);
        painter->drawPixmap(rect, pixmap(rect.size(), mode, state));
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override {
        Q_UNUSED(state);
        QByteArray tinted = svg_;
        const QString ink = (mode == QIcon::Disabled ? dimInk_ : ink_).name();
        tinted.replace("currentColor", ink.toLatin1());
        QSvgRenderer renderer(tinted);
        const qreal ratio = qApp != nullptr && qApp->primaryScreen() != nullptr
                                ? qApp->primaryScreen()->devicePixelRatio()
                                : 1.0;
        QPixmap out(size * ratio);
        out.setDevicePixelRatio(ratio);
        out.fill(Qt::transparent);
        QPainter painter(&out);
        renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(size)));
        return out;
    }

    QIconEngine *clone() const override { return new SvgEngine(name_, svg_, ink_, dimInk_); }
    QString key() const override { return QStringLiteral("svg-") + name_; }

private:
    QString name_;
    QByteArray svg_;
    QColor ink_;
    QColor dimInk_;
};
} // namespace

void ButtonIcon::apply(QAbstractButton *button, const QString &name) {
    button->setProperty("svgIcon", name);
    button->setIcon(icon(name));
    button->setIconSize(QSize(sidePx, sidePx));
    button->setText(QString());
}

void ButtonIcon::refresh(QWidget *root) {
    for (auto *button : root->findChildren<QAbstractButton *>()) {
        const QVariant name = button->property("svgIcon");
        if (name.isValid()) {
            button->setIcon(icon(name.toString()));
        }
    }
}

QIcon ButtonIcon::icon(const QString &name) {
    QFile file(QStringLiteral(":/icons/") + name + QStringLiteral(".svg"));
    const QByteArray svg = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    const QPalette palette = qApp != nullptr ? QApplication::palette() : QPalette();
    return QIcon(new SvgEngine(name, svg, palette.color(QPalette::ButtonText),
                               palette.color(QPalette::Disabled, QPalette::ButtonText)));
}
