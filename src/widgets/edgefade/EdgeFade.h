#pragma once

#include <QWidget>

#include <cstdint>

/// Scroll fade hint over a list edge: a gradient from the list
/// background to transparent, painted from the live palette (so theme
/// switches stay correct). Click-through. Created in code (it floats,
/// never takes layout space).
class EdgeFade : public QWidget {
public:
    enum class Side : std::uint8_t { Left, Right };
    explicit EdgeFade(Side side, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Side side;
};
