#include "graphwidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

namespace Perf
{

GraphWidget::GraphWidget(QWidget *parent)
    : QWidget(parent)
{
    this->setAutoFillBackground(false);
}

void GraphWidget::setHistory(const QVector<double> &data, double maxVal)
{
    if (data != this->m_data)
        ++this->m_historyTick;
    this->m_data   = data;
    this->m_maxVal = (maxVal > 0.0) ? maxVal : 100.0;
    this->update();
}

void GraphWidget::setSecondaryHistory(const QVector<double> &data2)
{
    this->m_data2 = data2;
    this->update();
}

void GraphWidget::setColor(QColor line, QColor fill)
{
    this->m_lineColor = line;
    this->m_fillColor = fill;
    this->update();
}

void GraphWidget::setSampleCapacity(int samples)
{
    this->m_sampleCapacity = qMax(2, samples);
    this->update();
}

void GraphWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = this->rect();
    const int   w = r.width();
    const int   h = r.height();

    // ── Background ────────────────────────────────────────────────────────────
    const QPalette pal = this->palette();
    const QColor bg = pal.color(QPalette::Base);
    p.fillRect(r, bg);

    // Fixed time axis slot geometry.
    const int sampleCount = qMax(2, this->m_sampleCapacity);
    const double stepX = static_cast<double>(w) / static_cast<double>(sampleCount - 1);

    // ── Grid ──────────────────────────────────────────────────────────────────
    QColor gridColor = pal.color(QPalette::Midlight);
    if (gridColor.alpha() == 255)
        gridColor.setAlpha(150);
    p.setPen(QPen(gridColor, 1));

    // Denser grid on larger widgets while keeping existing configured minimum.
    const int targetGridPxX = 80;
    const int targetGridPxY = 55;
    const int gridCols = qMax(this->m_gridCols, qMax(1, w / targetGridPxX));
    const int gridRows = qMax(this->m_gridRows, qMax(1, h / targetGridPxY));

    // Horizontal lines
    for (int i = 1; i < gridRows; ++i)
    {
        const int y = h * i / gridRows;
        p.drawLine(0, y, w, y);
    }

    // Vertical lines snap to time slots and phase-shift with sample updates.
    // This keeps graph points aligned to the same grid columns as data scrolls.
    const int gridSlotStep = qMax(1, sampleCount / gridCols);
    const int phase = this->m_historyTick % gridSlotStep;
    int lastX = -1;
    for (int slot = 1; slot < sampleCount - 1; ++slot)
    {
        if (((slot + phase) % gridSlotStep) != 0)
            continue;

        const int x = static_cast<int>(slot * stepX + 0.5);
        if (x <= 0 || x >= w || x == lastX)
            continue;
        p.drawLine(x, 0, x, h);
        lastX = x;
    }

    // ── Data ──────────────────────────────────────────────────────────────────
    if (this->m_data.isEmpty())
        return;

    const int n = this->m_data.size();

    // Keep a fixed-width time axis:
    // - when history is short, right-align it (empty area on the left)
    // - once full, new samples push older ones off the left edge
    const int visibleStart = qMax(0, n - sampleCount);
    const int visibleCount = n - visibleStart;
    const int slotOffset = qMax(0, sampleCount - visibleCount);

    // Build path — left to right, newest sample on the right
    QPainterPath path;
    for (int i = 0; i < visibleCount; ++i)
    {
        const double val = qBound(0.0, this->m_data.at(visibleStart + i), this->m_maxVal);
        const double fx  = (slotOffset + i) * stepX;
        const double fy  = h - (val / this->m_maxVal) * h;

        if (i == 0)
            path.moveTo(fx, fy);
        else
            path.lineTo(fx, fy);
    }

    // Filled area below the line (total user+kernel)
    QPainterPath fillPath = path;
    fillPath.lineTo((slotOffset + visibleCount - 1) * stepX, h);
    fillPath.lineTo(slotOffset * stepX, h);
    fillPath.closeSubpath();

    p.setPen(Qt::NoPen);
    p.setBrush(this->m_fillColor);
    p.drawPath(fillPath);

    // Kernel-time overlay (secondary data2) — drawn on top as a darker fill
    if (!this->m_data2.isEmpty())
    {
        const int n2 = this->m_data2.size();
        const int visibleStart2 = qMax(0, n2 - sampleCount);
        const int visibleCount2 = n2 - visibleStart2;
        const int slotOffset2 = qMax(0, sampleCount - visibleCount2);
        QPainterPath kPath;
        for (int i = 0; i < visibleCount2; ++i)
        {
            const double val = qBound(0.0, this->m_data2.at(visibleStart2 + i), this->m_maxVal);
            const double fx  = (slotOffset2 + i) * stepX;
            const double fy  = h - (val / this->m_maxVal) * h;
            if (i == 0)
                kPath.moveTo(fx, fy);
            else
                kPath.lineTo(fx, fy);
        }
        QPainterPath kFill = kPath;
        kFill.lineTo((slotOffset2 + visibleCount2 - 1) * stepX, h);
        kFill.lineTo(slotOffset2 * stepX, h);
        kFill.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(this->m_fillColor2);
        p.drawPath(kFill);
    }

    // The line itself
    p.setPen(QPen(this->m_lineColor, 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // ── Border ────────────────────────────────────────────────────────────────
    p.setPen(QPen(this->m_lineColor.darker(150), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(r.adjusted(0, 0, -1, -1));
}

} // namespace Perf
