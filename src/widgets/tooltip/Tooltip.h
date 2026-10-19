#pragma once

#include <QWidget>

class QEnterEvent;
class QTimer;

/// Generic hover card: tooltip-type window (never grabs, never
/// decorated — `xdg_popup` on Wayland, override-redirect on X11).
/// Elevation is server-drawn (the compositor shadows the window;
/// the window is exactly the card, no client effect, no margins).
/// Content-agnostic: subclasses build their content on [`card`] (or load a compiled
/// `.ui` via `setupUi` onto it) — the base creates no content of its
/// own. Outer widgets only call [`hover`] / [`unhover`] (and [`hide`]
/// on scroll/rebuild); delays, placement, and dismissal all live here.
class Tooltip : public QWidget {
    Q_OBJECT
public:
    /// Global hover-show delay (cards and tag tips alike): one
    /// constant so all hover feedback stays in sync.
    static constexpr int showDelayMs = 200;

    explicit Tooltip(QWidget *parent = nullptr);

    /// Hovered content: show after a short delay (instant switch
    /// while already open), so sweeps don't flicker.
    void hover();
    /// Left the content source: dismiss after a grace delay, so the
    /// cursor can reach the card (entry cancels it).
    void unhover();

protected:
    /// Styled card container: subclass content parents here.
    QWidget *card() { return tipCard; }
    /// Refresh visible content from subclass state (called on show
    /// and instant switches; no-op by default for static `.ui`s).
    virtual void refresh() {}
    /// Theme switch: stylesheet `palette()` roles bake at polish
    /// time (verified: a re-set is what re-resolves them), so the
    /// light card would survive into dark theme and vice versa.
    /// Re-set the card sheet and refresh live content.
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private slots:
    void onShowTimeout();
    void onHideTimeout();

private:
    /// Size to content, place near the cursor (clamped to the
    /// screen), and show.
    void placeAndShow();

    QTimer *showTimer = nullptr;
    QTimer *hideTimer = nullptr;
    QWidget *tipCard = nullptr;
};
