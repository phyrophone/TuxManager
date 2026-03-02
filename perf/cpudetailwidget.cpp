#include "cpudetailwidget.h"
#include "ui_cpudetailwidget.h"

#include <QFile>
#include <unistd.h>

namespace Perf
{

CpuDetailWidget::CpuDetailWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CpuDetailWidget)
{
    this->ui->setupUi(this);

    // CPU graph: blue
    this->ui->graphWidget->setColor(QColor(0x00, 0xbc, 0xff),
                                    QColor(0x00, 0x4c, 0x8a, 120));
    this->ui->graphWidget->setGridColumns(6);
    this->ui->graphWidget->setGridRows(4);

    // Populate static fields immediately
    const int logical = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    this->ui->statLogicalCpusValue->setText(QString::number(logical));
}

CpuDetailWidget::~CpuDetailWidget()
{
    delete this->ui;
}

void CpuDetailWidget::setProvider(PerfDataProvider *provider)
{
    if (this->m_provider)
        disconnect(this->m_provider, &PerfDataProvider::updated,
                   this, &CpuDetailWidget::onUpdated);

    this->m_provider = provider;

    if (this->m_provider)
    {
        connect(this->m_provider, &PerfDataProvider::updated,
                this, &CpuDetailWidget::onUpdated);
        this->onUpdated();
    }
}

void CpuDetailWidget::onUpdated()
{
    if (!this->m_provider)
        return;

    const double pct = this->m_provider->cpuPercent();

    this->ui->utilizationLabel->setText(QString::number(pct, 'f', 0) + "%");
    this->ui->statUtilValue->setText(QString::number(pct, 'f', 1) + "%");
    this->ui->graphWidget->setHistory(this->m_provider->cpuHistory());

    // Uptime from /proc/uptime
    QFile f("/proc/uptime");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        const double uptimeSec = f.readAll().simplified().split(' ').value(0).toDouble();
        f.close();
        const int days    = static_cast<int>(uptimeSec / 86400);
        const int hours   = static_cast<int>(uptimeSec / 3600)  % 24;
        const int minutes = static_cast<int>(uptimeSec / 60)    % 60;
        const int seconds = static_cast<int>(uptimeSec)         % 60;
        QString upStr;
        if (days > 0)
            upStr = tr("%1d %2:%3:%4")
                    .arg(days)
                    .arg(hours,   2, 10, QChar('0'))
                    .arg(minutes, 2, 10, QChar('0'))
                    .arg(seconds, 2, 10, QChar('0'));
        else
            upStr = tr("%1:%2:%3")
                    .arg(hours,   2, 10, QChar('0'))
                    .arg(minutes, 2, 10, QChar('0'))
                    .arg(seconds, 2, 10, QChar('0'));
        this->ui->statUptimeValue->setText(upStr);
    }
}

} // namespace Perf
