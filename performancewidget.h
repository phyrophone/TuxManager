#ifndef PERFORMANCEWIDGET_H
#define PERFORMANCEWIDGET_H

#include "perf/perfdataprovider.h"
#include "perf/sidepanel.h"
#include "perf/cpudetailwidget.h"
#include "perf/memorydetailwidget.h"
#include "perf/diskdetailwidget.h"
#include "perf/gpudetailwidget.h"

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
        void setActive(bool active);
        bool isActive() const { return this->m_active; }

    private slots:
        void onProviderUpdated();

    private:
        Ui::PerformanceWidget      *ui;

        Perf::PerfDataProvider     *m_provider;
        Perf::SidePanel            *m_sidePanel;
        QStackedWidget             *m_stack;
        Perf::CpuDetailWidget      *m_cpuDetail;
        Perf::MemoryDetailWidget   *m_memDetail;
        QVector<Perf::SidePanelItem *>   m_diskItems;
        QVector<Perf::DiskDetailWidget *> m_diskDetails;
        QVector<QString>                 m_diskNames;
        QVector<Perf::SidePanelItem *>   m_gpuItems;
        QVector<Perf::GpuDetailWidget *> m_gpuDetails;
        QVector<QString>                 m_gpuNames;
        bool                             m_active { false };

        enum PanelIndex { PanelCpu = 0, PanelMemory = 1 };

        void setupLayout();
        void setupSidePanel();
        void setupDiskPanels();
        void setupGpuPanels();
};

#endif // PERFORMANCEWIDGET_H
