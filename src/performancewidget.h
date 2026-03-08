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

#ifndef PERFORMANCEWIDGET_H
#define PERFORMANCEWIDGET_H

#include "perf/perfdataprovider.h"
#include "perf/sidepanel.h"
#include "perf/cpudetailwidget.h"
#include "perf/memorydetailwidget.h"
#include "perf/diskdetailwidget.h"
#include "perf/networkdetailwidget.h"
#include "perf/gpudetailwidget.h"
#include "perf/swapdetailwidget.h"

#include <QPoint>
#include <QStackedWidget>
#include <QVector>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class PerformanceWidget; }
QT_END_NAMESPACE

class PerformanceWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit PerformanceWidget(QWidget *parent = nullptr);
        ~PerformanceWidget();
        void SetActive(bool active);
        bool IsActive() const { return this->m_active; }

    private slots:
        void onProviderUpdated();
        void onSidePanelContextMenu(int index, const QPoint &globalPos);

    private:
        Ui::PerformanceWidget      *ui;

        Perf::PerfDataProvider     *m_provider;
        Perf::SidePanel            *m_sidePanel;
        QStackedWidget             *m_stack;
        Perf::CpuDetailWidget      *m_cpuDetail;
        Perf::MemoryDetailWidget   *m_memDetail;
        Perf::SwapDetailWidget     *m_swapDetail;
        QVector<Perf::SidePanelItem *>   m_diskItems;
        QVector<Perf::DiskDetailWidget *> m_diskDetails;
        QVector<QString>                 m_diskNames;
        QVector<Perf::SidePanelItem *>   m_networkItems;
        QVector<Perf::NetworkDetailWidget *> m_networkDetails;
        QVector<QString>                 m_networkNames;
        QVector<Perf::SidePanelItem *>   m_gpuItems;
        QVector<Perf::GpuDetailWidget *> m_gpuDetails;
        QVector<QString>                 m_gpuNames;
        bool                             m_active { false };
        int                              m_cpuPanelIndex { -1 };
        int                              m_memoryPanelIndex { -1 };
        int                              m_swapPanelIndex { -1 };
        int                              m_diskPanelStart { -1 };
        int                              m_networkPanelStart { -1 };
        int                              m_gpuPanelStart { -1 };

        void setupLayout();
        void setupSidePanel();
        void setupDiskPanels();
        void setupNetworkPanels();
        void setupGpuPanels();
        void applyPanelVisibility();
        void updateSamplingPolicy();
        bool anyPanelVisibleAfterToggle(bool cpu,
                                        bool memory,
                                        bool swap,
                                        bool disks,
                                        bool network,
                                        bool gpu) const;
        static QString formatNetRate(double bytesPerSec);
};

#endif // PERFORMANCEWIDGET_H
