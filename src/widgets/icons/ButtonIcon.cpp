#include "widgets/icons/ButtonIcon.h"

#include <QAbstractButton>
#include <QApplication>
#include <QFile>
#include <QIconEngine>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QScreen>
#include <QStyleOptionButton>
#include <QSvgRenderer>
#include <QWidget>

namespace {
/// SVG icon engine tinting `currentColor` artwork at render time
/// (normal + dimmed-disabled inks ride along, so no per-mode
/// assets; the checked-On state gets its own ink contrasting the
/// style's real checked-button fill — see `ButtonIcon::icon`).
/// Distinct `key()` per file and ink: sharing one would alias
/// every icon (and every re-tint) to the same cache entry.
class SvgEngine : public QIconEngine {
public:
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    SvgEngine(QString name, QByteArray svg, QColor ink, QColor dimInk, QColor onInk,
              QColor dimOnInk)
        : name_(std::move(name)), svg_(std::move(svg)), ink_(ink), dimInk_(dimInk), onInk_(onInk),
          dimOnInk_(dimOnInk) {}

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode,
               QIcon::State state) override {
        painter->drawPixmap(rect, pixmap(rect.size(), mode, state));
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override {
        const QColor ink = mode == QIcon::Disabled ? (state == QIcon::On ? dimOnInk_ : dimInk_)
                                                   : (state == QIcon::On ? onInk_ : ink_);
        QByteArray tinted = svg_;
        tinted.replace("currentColor", ink.name().toLatin1());
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

    QIconEngine *clone() const override {
        return new SvgEngine(name_, svg_, ink_, dimInk_, onInk_, dimOnInk_);
    }
    QString key() const override {
        return QStringLiteral("svg-") + name_ + ink_.name() + dimInk_.name() + onInk_.name() +
               dimOnInk_.name();
    }

private:
    QString name_;
    QByteArray svg_;
    QColor ink_;
    QColor dimInk_;
    QColor onInk_;
    QColor dimOnInk_;
};

/// What a flat button paints under itself, checked or not (all
/// icon buttons are flat): probed offscreen through the style
/// itself. Transparent when the style paints nothing.
QColor flatBevelFill(bool checked) {
    QPushButton probe;
    probe.setFlat(true);
    probe.setCheckable(true);
    probe.setChecked(checked);
    probe.resize(40, 40);
    probe.ensurePolished();
    QPixmap pm(probe.size());
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    QStyleOptionButton option;
    option.initFrom(&probe);
    if (checked) {
        option.state |= QStyle::State_On;
    } else {
        option.state &= ~QStyle::State_On;
    }
    option.rect = QRect(QPoint(0, 0), pm.size());
    probe.style()->drawControl(QStyle::CE_PushButtonBevel, &option, &painter, &probe);
    painter.end();
    return pm.toImage().pixelColor(pm.rect().center());
}

/// What the style paints under a checked flat button: `flatBevelFill(true)`,
/// transparent when the style paints nothing.
QColor checkedFlatFill() { return flatBevelFill(true); }
} // namespace

QColor ButtonIcon::contrastingInk(const QColor &base, const QColor &background) {
    // Keep the theme ink when it already stands out from the
    // background; otherwise flip its lightness (a white icon on
    // a white checked button comes out dark, and vice versa).
    if (qAbs(base.lightness() - background.lightness()) >= 96) {
        return base;
    }
    QColor flipped = base;
    flipped.setHsl(flipped.hslHue(), flipped.hslSaturation(), 255 - flipped.lightness());
    return flipped;
}

QColor ButtonIcon::checkedInk(const QColor &ink, const QColor &checkedFill,
                              const QColor &uncheckedFill, const QColor &highlight,
                              const QColor &window) {
    // A distinct checked fill carries the state on its own: the
    // glyph only needs to stay readable on it.
    if (qAbs(checkedFill.lightness() - uncheckedFill.lightness()) >= 24) {
        return contrastingInk(ink, checkedFill);
    }
    // No background cue (checked paints like unchecked): the glyph
    // itself carries the state in the accent color, when the
    // accent stands out from the window behind it.
    if (qAbs(highlight.lightness() - window.lightness()) >= 64) {
        return highlight;
    }
    return contrastingInk(ink, checkedFill);
}

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
    const QColor ink = palette.color(QPalette::ButtonText);
    const QColor dimInk = palette.color(QPalette::Disabled, QPalette::ButtonText);
    // Checked fill from the style (white under Kvantum): the
    // On-state ink contrasts with it, the Off state keeps the
    // theme ink (checked buttons are the only ones whose
    // background leaves the window color). When the style paints
    // no distinct checked fill (windows11 flat buttons), the
    // accent-colored glyph carries the checked state instead.
    QColor checked = checkedFlatFill();
    if (!checked.alpha()) {
        checked = palette.color(QPalette::Window);
    }
    QColor unchecked = flatBevelFill(false);
    if (!unchecked.alpha()) {
        unchecked = palette.color(QPalette::Window);
    }
    const QColor onInk = checkedInk(ink, checked, unchecked, palette.color(QPalette::Highlight),
                                    palette.color(QPalette::Window));
    const QColor onDimInk =
        checkedInk(dimInk, checked, unchecked, palette.color(QPalette::Highlight),
                   palette.color(QPalette::Window));
    return QIcon(new SvgEngine(name, svg, ink, dimInk, onInk, onDimInk));
}
