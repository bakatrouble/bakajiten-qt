#include "ui/mainwindow/MainWindow.h"

#include "ui_MainWindow.h"

#include "ui/dictionary/DictionaryWidget.h"
#include "ui/recognizer/RecognizerWidget.h"

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QObject>
#include <QPoint>
#include <QString>
#include <QWidget>
#include <Qt>

/// Click-away / focus-loss auto-hide policy: an open panel hides on
/// any press or focus move outside itself, with four exemptions —
/// presses inside it, the open button, the search field (focused
/// while the panel is open; clicks there only move the cursor), and
/// menus (which handle their own input).
static bool isInPanel(const RecognizerWidget *panel, const QPoint &global) {
    return panel->rect().contains(panel->mapFromGlobal(global));
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        // Type-to-search: printable keys pressed anywhere in the
        // window (table, article, panel, buttons) focus the query
        // field and redeliver there, so no keystroke is lost.
        // Modifier combos (shortcuts), unprintables (arrows, tab,
        // return), other windows (dialogs, menus), and text fields
        // keep native handling.
        const auto *ke = static_cast<const QKeyEvent *>(event);
        const bool modified =
            (ke->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) != 0;
        const QString text = ke->text();
        QWidget *focus = QApplication::focusWidget();
        QLineEdit *field = ui->centralwidget->searchField();
        if (!modified && text.size() == 1 && text.at(0).isPrint() && focus != nullptr &&
            focus->window() == this && qobject_cast<QLineEdit *>(focus) == nullptr &&
            qobject_cast<QMenuBar *>(focus) == nullptr && field != nullptr && field->isVisible() &&
            field->isEnabled()) {
            field->setFocus(Qt::OtherFocusReason);
            QApplication::sendEvent(field, event);
            return true;
        }
    }
    if (event->type() == QEvent::MouseButtonPress && recognizer->isVisible()) {
        // Menus handle themselves.
        if (qobject_cast<QMenu *>(watched) != nullptr ||
            qobject_cast<QMenuBar *>(watched) != nullptr) {
            return false;
        }
        const auto *me = static_cast<const QMouseEvent *>(event);
        const QPoint global = me->globalPosition().toPoint();
        if (isInPanel(recognizer, global) || ui->centralwidget->isOnRecognizerButton(global) ||
            ui->centralwidget->isOnSearchInput(global)) {
            return false;
        }
        setRecognizerVisible(false);
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::changeEvent(QEvent *event) {
    if (event->type() == QEvent::FontChange) {
        ui->menubar->updateGeometry();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::onFocusChanged(QWidget * /*old*/, QWidget *now) {
    if (!recognizer->isVisible()) {
        return;
    }
    if (now == nullptr) {
        setRecognizerVisible(false);
        return;
    }
    // Same exemptions as click-away: the panel (and its children),
    // the open button, the search field, and menus must survive.
    if (qobject_cast<QMenu *>(now) != nullptr || qobject_cast<QMenuBar *>(now) != nullptr) {
        return;
    }
    QWidget *w = now;
    while (w != nullptr) {
        if (w == recognizer) {
            return;
        }
        w = w->parentWidget();
    }
    const QPoint global = now->mapToGlobal(now->rect().center());
    if (ui->centralwidget->isOnRecognizerButton(global) ||
        ui->centralwidget->isOnSearchInput(global)) {
        return;
    }
    setRecognizerVisible(false);
}
