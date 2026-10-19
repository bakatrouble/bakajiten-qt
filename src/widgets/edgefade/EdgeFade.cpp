#include "widgets/edgefade/EdgeFade.h"

#include "core/palette/MaterialPalette.h"

#include <QLinearGradient>
#include <QPainter>
#include <QWidget>

EdgeFade::EdgeFade(Side side, QWidget *parent) : QWidget(parent), side(side) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void EdgeFade::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    const MaterialPalette theme = MaterialPalette::fromPalette(palette());
    QLinearGradient gradient;
    if (side == Side::Left) {
        gradient = QLinearGradient(QPointF(0, 0), QPointF(width(), 0));
    } else {
        gradient = QLinearGradient(QPointF(width(), 0), QPointF(0, 0));
    }
    gradient.setColorAt(0, theme.base());
    gradient.setColorAt(1, theme.fadeEnd());
    painter.fillRect(rect(), gradient);
}
