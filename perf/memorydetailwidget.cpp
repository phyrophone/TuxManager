#include "memorydetailwidget.h"
#include "ui_memorydetailwidget.h"

namespace Perf
{

MemoryDetailWidget::MemoryDetailWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MemoryDetailWidget)
{
    this->ui->setupUi(this);

    // Memory graph: purple / magenta
    this->ui->graphWidget->setColor(QColor(0xcc, 0x44, 0xcc),
                                    QColor(0x66, 0x11, 0x66, 130));
    this->ui->graphWidget->setGridColumns(6);
    this->ui->graphWidget->setGridRows(4);
}

MemoryDetailWidget::~MemoryDetailWidget()
{
    delete this->ui;
}

void MemoryDetailWidget::setProvider(PerfDataProvider *provider)
{
    if (this->m_provider)
        disconnect(this->m_provider, &PerfDataProvider::updated,
                   this, &MemoryDetailWidget::onUpdated);

    this->m_provider = provider;

    if (this->m_provider)
    {
        connect(this->m_provider, &PerfDataProvider::updated,
                this, &MemoryDetailWidget::onUpdated);
        this->onUpdated();
    }
}

void MemoryDetailWidget::onUpdated()
{
    if (!this->m_provider)
        return;

    const qint64 total   = this->m_provider->memTotalKb();
    const qint64 used    = this->m_provider->memUsedKb();
    const qint64 avail   = this->m_provider->memAvailKb();
    const qint64 cached  = this->m_provider->memCachedKb();
    const qint64 buffers = this->m_provider->memBuffersKb();

    this->ui->totalLabel->setText(fmtGb(total) + " GB");
    this->ui->statInUseValue->setText(fmtGb(used)    + " GB");
    this->ui->statAvailValue->setText(fmtGb(avail)   + " GB");
    this->ui->statCachedValue->setText(fmtGb(cached) + " GB");
    this->ui->statBuffersValue->setText(fmtGb(buffers) + " GB");

    // Composition bar (0–1000 scale)
    if (total > 0)
    {
        const int barVal = static_cast<int>(
            static_cast<double>(used) / static_cast<double>(total) * 1000.0);
        this->ui->compositionBar->setValue(barVal);
    }

    this->ui->graphWidget->setHistory(this->m_provider->memHistory());
}

// static
QString MemoryDetailWidget::fmtGb(qint64 kb)
{
    const double gb = static_cast<double>(kb) / (1024.0 * 1024.0);
    if (gb >= 10.0)
        return QString::number(gb, 'f', 1);
    return QString::number(gb, 'f', 2);
}

} // namespace Perf
