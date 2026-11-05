#include "ui/recognizer/RecognizerWidget.h"

#include "ui_RecognizerWidget.h"

#include "core/palette/MaterialPalette.h"
#include "core/settings/MemorizedState.h"
#include "widgets/icons/ButtonIcon.h"
#include "widgets/list/RecognizerCandidateList.h"
#include "widgets/toolbutton/ToolButtonFit.h"

#include "core/settings/MemorizedState.h"

#include <QCloseEvent>
#include <QEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>

void RecognizerWidget::applyPanelSize(int reqW, int reqH) {
    // Free resize: clamp the request to the layout minimum and the
    // parent bounds, no shape forcing.
    const QSize need = layout()->minimumSize();
    QSize max(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    if (const QWidget *par = parentWidget()) {
        max.setWidth(par->width() - pos().x());
        max.setHeight(par->height() - pos().y());
    }
    resize(
        QSize(qBound(need.width(), reqW, max.width()), qBound(need.height(), reqH, max.height())));
}

void RecognizerWidget::paintEvent(QPaintEvent *) {
    // Card look: filled background a touch above the window color,
    // 1px border, rounded corners. Unpainted corners show the parent
    // through (plain child, no translucency attribute needed).
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const MaterialPalette chrome = MaterialPalette::fromPalette(palette());
    p.setBrush(chrome.cardFill());
    p.setPen(QPen(chrome.cardBorder(), 1));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 8, 8);
    // Resize grip: triangular corner over the grip label (the label
    // itself stays empty: cursor + drag handling only). Corner and
    // legs fill the label exactly, in the text color.
    const QRect grip(ui->resizeGrip->mapTo(this, QPoint(0, 0)), ui->resizeGrip->size());
    const QPoint corner = grip.bottomRight();
    const int leg = qMin(grip.width(), grip.height());
    const QPolygon triangle(
        {QPoint(corner.x() - leg, corner.y()), corner, QPoint(corner.x(), corner.y() - leg)});
    p.setPen(Qt::NoPen);
    p.setBrush(chrome.windowText());
    p.drawPolygon(triangle);
}

void RecognizerWidget::closeEvent(QCloseEvent *event) {
    event->accept();
    hide();
    emit panelClosed();
}

void RecognizerWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    MemorizedState::setPanelSize(size());
}

void RecognizerWidget::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ThemeChange) {
        ButtonIcon::refresh(this);
    }
    if (event->type() == QEvent::FontChange) {
        fitToolButtons(this, chromeButton);
        ui->candidatesList->refreshScale();
        // Rows just moved: refit the cells once the layout settles.
        QTimer::singleShot(0, this, [this] { ui->candidatesList->refreshGrid(); });
    }
}

void RecognizerWidget::onGridToggled(bool on) { ui->canvas->setGridVisible(on); }
