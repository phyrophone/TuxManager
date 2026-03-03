#ifndef PERF_GPUDETAILWIDGET_H
#define PERF_GPUDETAILWIDGET_H

#include "graphwidget.h"
#include "perfdataprovider.h"

#include <QComboBox>
#include <QLabel>
#include <QVector>
#include <QWidget>

namespace Perf
{

class GpuDetailWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit GpuDetailWidget(QWidget *parent = nullptr);

        void setProvider(PerfDataProvider *provider);
        void setGpuIndex(int index);

    private slots:
        void onUpdated();
        void onEngineSelectionChanged(int slot, int comboIndex);

    private:
        static QString formatMemMib(qint64 mib);
        static QString formatRate(double bytesPerSec);

        PerfDataProvider *m_provider { nullptr };
        int               m_gpuIndex { -1 };

        QLabel *m_titleLabel { nullptr };
        QLabel *m_modelLabel { nullptr };
        QLabel *m_utilValueLabel { nullptr };
        QLabel *m_gpuMemValueLabel { nullptr };
        QLabel *m_dedicatedMemValueLabel { nullptr };
        QLabel *m_sharedMemValueLabel { nullptr };
        QLabel *m_driverValueLabel { nullptr };
        QLabel *m_backendValueLabel { nullptr };
        QLabel *m_dedicatedMemGraphMaxLabel { nullptr };
        QLabel *m_sharedMemGraphMaxLabel { nullptr };
        QLabel *m_copyBwGraphMaxLabel { nullptr };
        QLabel *m_copyBwLegendLabel { nullptr };

        QVector<QComboBox *>  m_engineSelectors;
        QVector<QLabel *>     m_engineValueLabels;
        QVector<GraphWidget *> m_engineGraphs;
        QVector<int>           m_selectedEngineBySlot;

        GraphWidget *m_dedicatedMemGraph { nullptr };
        GraphWidget *m_sharedMemGraph { nullptr };
        GraphWidget *m_copyBwGraph { nullptr };
        QVector<double> m_sharedMemHistory;

        void rebuildEngineSelectors();
};

} // namespace Perf

#endif // PERF_GPUDETAILWIDGET_H
