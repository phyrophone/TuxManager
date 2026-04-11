/*
 * Tux Manager - Linux system monitor
 * Copyright (C) 2026 Petr Bena <petr@bena.rocks>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef PERF_SWAPGRAPHAREA_H
#define PERF_SWAPGRAPHAREA_H

#include <QVector>
#include <QWidget>

class QGridLayout;
class QStackedWidget;

namespace Perf
{
    class GraphWidget;

    class SwapGraphArea : public QWidget
    {
        Q_OBJECT

        public:
            enum class GraphMode { Overall, PerDevice };

            explicit SwapGraphArea(QWidget *parent = nullptr);

            void Init();
            void SetMode(GraphMode mode);
            GraphMode GetMode() const { return this->m_mode; }
            void ApplyColorScheme();
            void UpdateData();
            void RebindDevices();

        signals:
            void contextMenuRequested(const QPoint &globalPos);

        protected:
            void contextMenuEvent(QContextMenuEvent *event) override;

        private:
            void ensureDeviceGraphs(int count);
            void bindOverallGraphSources();
            void bindDeviceGraphSources(int count);

            QStackedWidget         *m_stack { nullptr };
            GraphWidget            *m_overallGraph { nullptr };
            QWidget                *m_perDeviceContainer { nullptr };
            QGridLayout            *m_perDeviceGrid { nullptr };
            QVector<GraphWidget *>  m_deviceGraphs;

            GraphMode               m_mode { GraphMode::Overall };
    };
} // namespace Perf

#endif // PERF_SWAPGRAPHAREA_H
