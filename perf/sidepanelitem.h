#ifndef PERF_SIDEPANELITEM_H
#define PERF_SIDEPANELITEM_H

#include "graphwidget.h"

#include <QVector>
#include <QWidget>

namespace Perf
{

/// One entry in the left-hand side panel of the Performance tab.
/// Displays a title, a subtitle (e.g. "3% 3.40 GHz"), and an embedded
/// mini sparkline graph.  Emits clicked() when the user presses it.
class SidePanelItem : public QWidget
{
    Q_OBJECT

    public:
        explicit SidePanelItem(const QString &title,
                               QWidget       *parent = nullptr);

        /// Push a fresh data snapshot into the embedded mini-graph and
        /// update the subtitle text.
        void update(const QString        &subtitle,
                    const QVector<double> &history,
                    double                maxVal = 100.0);

        void setSelected(bool selected);
        bool isSelected() const { return this->m_selected; }

        void setGraphColor(QColor line, QColor fill);

        QSize sizeHint()        const override { return QSize(160, 80); }
        QSize minimumSizeHint() const override { return QSize(120, 70); }

    signals:
        void clicked();

    protected:
        void paintEvent(QPaintEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void enterEvent(QEnterEvent *event) override;
        void leaveEvent(QEvent *event) override;

    private:
        QString      m_title;
        QString      m_subtitle;
        GraphWidget *m_graph;
        bool         m_selected { false };
        bool         m_hovered  { false };
};

} // namespace Perf

#endif // PERF_SIDEPANELITEM_H
