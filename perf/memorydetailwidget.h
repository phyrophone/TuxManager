#ifndef PERF_MEMORYDETAILWIDGET_H
#define PERF_MEMORYDETAILWIDGET_H

#include "perfdataprovider.h"

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class MemoryDetailWidget; }
QT_END_NAMESPACE

namespace Perf
{

class MemoryDetailWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit MemoryDetailWidget(QWidget *parent = nullptr);
        ~MemoryDetailWidget();

        void setProvider(PerfDataProvider *provider);

    private slots:
        void onUpdated();

    private:
        Ui::MemoryDetailWidget *ui;
        PerfDataProvider       *m_provider { nullptr };

        static QString fmtGb(qint64 kb);
};

} // namespace Perf

#endif // PERF_MEMORYDETAILWIDGET_H
