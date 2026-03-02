#ifndef PERFORMANCEWIDGET_H
#define PERFORMANCEWIDGET_H

#include "perf/perfdataprovider.h"
#include "perf/sidepanel.h"
#include "perf/cpudetailwidget.h"
#include "perf/memorydetailwidget.h"

#include <QStackedWidget>
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

    private slots:
        void onProviderUpdated();

    private:
        Ui::PerformanceWidget      *ui;

        Perf::PerfDataProvider     *m_provider;
        Perf::SidePanel            *m_sidePanel;
        QStackedWidget             *m_stack;
        Perf::CpuDetailWidget      *m_cpuDetail;
        Perf::MemoryDetailWidget   *m_memDetail;

        enum PanelIndex { PanelCpu = 0, PanelMemory = 1 };

        void setupLayout();
        void setupSidePanel();
};

#endif // PERFORMANCEWIDGET_H
