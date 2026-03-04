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

#include "performancewidget.h"
#include "ui_performancewidget.h"

#include "configuration.h"
#include "logger.h"

#include <QHBoxLayout>

// ── Construction ──────────────────────────────────────────────────────────────

PerformanceWidget::PerformanceWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PerformanceWidget)
    , m_provider(new Perf::PerfDataProvider(this))
    , m_sidePanel(new Perf::SidePanel(this))
    , m_stack(new QStackedWidget(this))
    , m_cpuDetail(new Perf::CpuDetailWidget(this))
    , m_memDetail(new Perf::MemoryDetailWidget(this))
{
    this->ui->setupUi(this);

    this->setupLayout();
    this->setupSidePanel();

    // Wire detail widgets to the data provider
    this->m_cpuDetail->setProvider(this->m_provider);
    this->m_memDetail->setProvider(this->m_provider);

    // Update side panel thumbnails on every sample
    connect(this->m_provider, &Perf::PerfDataProvider::updated, this, &PerformanceWidget::onProviderUpdated);

    // Expensive process/thread counting is only needed for CPU detail page.
    connect(this->m_sidePanel, &Perf::SidePanel::currentChanged, this, [this](int index)
    {
        this->m_provider->SetProcessStatsEnabled(index == PanelCpu);
    });
    this->m_provider->SetProcessStatsEnabled(this->m_sidePanel->GetCurrentIndex() == PanelCpu);

    this->SetActive(false);

    LOG_DEBUG("PerformanceWidget initialised");
}

PerformanceWidget::~PerformanceWidget()
{
    delete this->ui;
}

// ── Private setup ─────────────────────────────────────────────────────────────

void PerformanceWidget::setupLayout()
{
    // The .ui gives us a bare QHBoxLayout (horizontalLayout) — populate it.
    QHBoxLayout *lay = qobject_cast<QHBoxLayout *>(this->layout());

    lay->addWidget(this->m_sidePanel);

    // Thin separator line
    QFrame *separator = new QFrame(this);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    lay->addWidget(separator);

    lay->addWidget(this->m_stack, /*stretch=*/1);
}

void PerformanceWidget::setupSidePanel()
{
    // ── CPU item ─────────────────────────────────────────────────────────────
    auto *cpuItem = new Perf::SidePanelItem(tr("CPU"), this);
    cpuItem->SetGraphColor(QColor(0x00, 0xbc, 0xff), QColor(0x00, 0x4c, 0x8a, 120));
    this->m_sidePanel->AddItem(cpuItem);
    this->m_stack->addWidget(this->m_cpuDetail);

    // ── Memory item ──────────────────────────────────────────────────────────
    auto *memItem = new Perf::SidePanelItem(tr("Memory"), this);
    memItem->SetGraphColor(QColor(0xcc, 0x44, 0xcc), QColor(0x66, 0x11, 0x66, 130));
    this->m_sidePanel->AddItem(memItem);
    this->m_stack->addWidget(this->m_memDetail);

    this->setupDiskPanels();
    this->setupNetworkPanels();
    this->setupGpuPanels();

    // Side-panel selection drives the stacked widget page
    connect(this->m_sidePanel, &Perf::SidePanel::currentChanged, this->m_stack, &QStackedWidget::setCurrentIndex);
}

void PerformanceWidget::setupDiskPanels()
{
    const int count = this->m_provider->DiskCount();
    for (int i = 0; i < count; ++i)
    {
        const QString devName = this->m_provider->DiskName(i);
        this->m_diskNames.append(devName);

        auto *item = new Perf::SidePanelItem(tr("Disk (%1)").arg(devName), this);
        item->SetGraphColor(QColor(0x66, 0xbb, 0x44), QColor(0x33, 0x66, 0x22, 120));
        this->m_sidePanel->AddItem(item);
        this->m_diskItems.append(item);

        auto *detail = new Perf::DiskDetailWidget(this);
        detail->SetProvider(this->m_provider);
        detail->SetDiskIndex(i);
        this->m_stack->addWidget(detail);
        this->m_diskDetails.append(detail);
    }
}

void PerformanceWidget::setupGpuPanels()
{
    const int count = this->m_provider->GpuCount();
    for (int i = 0; i < count; ++i)
    {
        const QString GpuName = this->m_provider->GpuName(i);
        this->m_gpuNames.append(GpuName);

        auto *item = new Perf::SidePanelItem(tr("GPU %1").arg(i), this);
        item->SetGraphColor(QColor(0x44, 0xa8, 0xff), QColor(0x1e, 0x4d, 0x82, 110));
        this->m_sidePanel->AddItem(item);
        this->m_gpuItems.append(item);

        auto *detail = new Perf::GpuDetailWidget(this);
        detail->SetProvider(this->m_provider);
        detail->SetGpuIndex(i);
        this->m_stack->addWidget(detail);
        this->m_gpuDetails.append(detail);
    }
}

void PerformanceWidget::setupNetworkPanels()
{
    const int count = this->m_provider->NetworkCount();
    for (int i = 0; i < count; ++i)
    {
        const QString ifName = this->m_provider->NetworkName(i);
        this->m_networkNames.append(ifName);

        auto *item = new Perf::SidePanelItem(tr("NIC (%1)").arg(ifName), this);
        item->SetGraphColor(QColor(0xdb, 0x8b, 0x3a), QColor(0x66, 0x3f, 0x1f, 110));
        this->m_sidePanel->AddItem(item);
        this->m_networkItems.append(item);

        auto *detail = new Perf::NetworkDetailWidget(this);
        detail->SetProvider(this->m_provider);
        detail->SetNetworkIndex(i);
        this->m_stack->addWidget(detail);
        this->m_networkDetails.append(detail);
    }
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void PerformanceWidget::onProviderUpdated()
{
    // Update CPU side panel item
    const double cpuPct = this->m_provider->CpuPercent();
    const QString cpuSub = QString::number(cpuPct, 'f', 0) + "%";
    if (auto *item = this->m_sidePanel->GetItemAt(PanelCpu))
        item->Update(cpuSub, this->m_provider->CpuHistory());

    // Update Memory side panel item
    const qint64 used  = this->m_provider->MemUsedKb();
    const qint64 total = this->m_provider->MemTotalKb();
    const double usedGb  = static_cast<double>(used)  / (1024.0 * 1024.0);
    const double totalGb = static_cast<double>(total) / (1024.0 * 1024.0);
    const int    pct     = total > 0
                           ? static_cast<int>(static_cast<double>(used) / total * 100.0)
                           : 0;
    const QString memSub = QString("%1/%2 GB (%3%)")
                           .arg(usedGb,  0, 'f', 1)
                           .arg(totalGb, 0, 'f', 1)
                           .arg(pct);
    if (auto *item = this->m_sidePanel->GetItemAt(PanelMemory))
        item->Update(memSub, this->m_provider->MemHistory());

    for (int i = 0; i < this->m_diskItems.size(); ++i)
    {
        if (i >= this->m_provider->DiskCount())
            break;
        auto *item = this->m_diskItems.at(i);
        if (!item)
            continue;

        const QString diskSub = tr("%1 %2")
                                .arg(this->m_provider->DiskType(i))
                                .arg(QString::number(this->m_provider->DiskActivePercent(i), 'f', 0) + "%");
        item->Update(diskSub, this->m_provider->DiskActiveHistory(i));
    }

    for (int i = 0; i < this->m_gpuItems.size(); ++i)
    {
        if (i >= this->m_provider->GpuCount())
            break;
        auto *item = this->m_gpuItems.at(i);
        if (!item)
            continue;

        const QString gpuSub = tr("%1%2")
                               .arg(QString::number(this->m_provider->GpuUtilPercent(i), 'f', 0))
                               .arg("%");
        item->Update(gpuSub, this->m_provider->GpuUtilHistory(i));
    }

    for (int i = 0; i < this->m_networkItems.size(); ++i)
    {
        if (i >= this->m_provider->NetworkCount())
            break;
        auto *item = this->m_networkItems.at(i);
        if (!item)
            continue;

        const double tx = this->m_provider->NetworkTxBytesPerSec(i);
        const double rx = this->m_provider->NetworkRxBytesPerSec(i);
        const QVector<double> &rxHistory = this->m_provider->NetworkRxHistory(i);
        const QVector<double> &txHistory = this->m_provider->NetworkTxHistory(i);
        const QString netSub = tr("U:%1 D:%2")
                               .arg(formatNetRate(tx))
                               .arg(formatNetRate(rx));
        double maxRate = 1024.0; // keep at least 1KB/s visual range
        for (double v : rxHistory)
            maxRate = qMax(maxRate, v);
        for (double v : txHistory)
            maxRate = qMax(maxRate, v);
        item->Update(netSub, rxHistory, maxRate);
    }
}

void PerformanceWidget::SetActive(bool active)
{
    if (this->m_active == active)
        return;

    this->m_active = active;
    this->m_provider->SetActive(active);
    if (active)
        this->onProviderUpdated();
}

QString PerformanceWidget::formatNetRate(double bytesPerSec)
{
    if (bytesPerSec >= 1024.0 * 1024.0)
        return QString::number(bytesPerSec / (1024.0 * 1024.0), 'f', 1) + tr("M/s");
    if (bytesPerSec >= 1024.0)
        return QString::number(bytesPerSec / 1024.0, 'f', 0) + tr("K/s");
    return QString::number(bytesPerSec, 'f', 0) + tr("B/s");
}
