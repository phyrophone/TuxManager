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

#include <QAction>
#include <QHBoxLayout>
#include <QMenu>

// ── Construction ──────────────────────────────────────────────────────────────

PerformanceWidget::PerformanceWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PerformanceWidget)
    , m_provider(new Perf::PerfDataProvider(this))
    , m_sidePanel(new Perf::SidePanel(this))
    , m_stack(new QStackedWidget(this))
    , m_cpuDetail(new Perf::CpuDetailWidget(this))
    , m_memDetail(new Perf::MemoryDetailWidget(this))
    , m_swapDetail(new Perf::SwapDetailWidget(this))
{
    this->ui->setupUi(this);

    this->setupLayout();
    this->setupSidePanel();

    // Wire detail widgets to the data provider
    this->m_cpuDetail->setProvider(this->m_provider);
    this->m_memDetail->setProvider(this->m_provider);
    this->m_swapDetail->SetProvider(this->m_provider);

    // Update side panel thumbnails on every sample
    connect(this->m_provider, &Perf::PerfDataProvider::updated, this, &PerformanceWidget::onProviderUpdated);

    // Expensive process/thread counting is only needed for CPU detail page.
    connect(this->m_sidePanel, &Perf::SidePanel::currentChanged, this, [this](int index)
    {
        this->m_provider->SetProcessStatsEnabled(index == this->m_cpuPanelIndex && CFG->PerfShowCpu);
    });
    connect(this->m_sidePanel, &Perf::SidePanel::itemContextMenuRequested,
            this, &PerformanceWidget::onSidePanelContextMenu);

    this->applyPanelVisibility();
    this->updateSamplingPolicy();
    this->m_provider->SetProcessStatsEnabled(this->m_sidePanel->GetCurrentIndex() == this->m_cpuPanelIndex
                                             && CFG->PerfShowCpu);

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
    this->m_cpuPanelIndex = this->m_sidePanel->AddItem(cpuItem);
    this->m_stack->addWidget(this->m_cpuDetail);

    // ── Memory item ──────────────────────────────────────────────────────────
    auto *memItem = new Perf::SidePanelItem(tr("Memory"), this);
    memItem->SetGraphColor(QColor(0xcc, 0x44, 0xcc), QColor(0x66, 0x11, 0x66, 130));
    this->m_memoryPanelIndex = this->m_sidePanel->AddItem(memItem);
    this->m_stack->addWidget(this->m_memDetail);

    // ── Swap item ────────────────────────────────────────────────────────────
    auto *swapItem = new Perf::SidePanelItem(tr("Swap"), this);
    swapItem->SetGraphColor(QColor(0xcc, 0x88, 0x44), QColor(0x66, 0x33, 0x11, 120));
    this->m_swapPanelIndex = this->m_sidePanel->AddItem(swapItem);
    this->m_stack->addWidget(this->m_swapDetail);

    this->setupDiskPanels();
    this->setupNetworkPanels();
    this->setupGpuPanels();

    // Side-panel selection drives the stacked widget page
    connect(this->m_sidePanel, &Perf::SidePanel::currentChanged, this->m_stack, &QStackedWidget::setCurrentIndex);
}

void PerformanceWidget::setupDiskPanels()
{
    this->m_diskPanelStart = this->m_sidePanel->GetCount();
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
    this->m_gpuPanelStart = this->m_sidePanel->GetCount();
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
    this->m_networkPanelStart = this->m_sidePanel->GetCount();
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
    if (CFG->PerfShowCpu)
    {
        if (auto *item = this->m_sidePanel->GetItemAt(this->m_cpuPanelIndex))
            item->Update(cpuSub, this->m_provider->CpuHistory());
    }

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
    if (CFG->PerfShowMemory)
    {
        if (auto *item = this->m_sidePanel->GetItemAt(this->m_memoryPanelIndex))
            item->Update(memSub, this->m_provider->MemHistory());
    }

    // Update Swap side panel item
    const qint64 swapUsed = this->m_provider->SwapUsedKb();
    const qint64 swapTotal = this->m_provider->SwapTotalKb();
    const double swapUsedGb = static_cast<double>(swapUsed) / (1024.0 * 1024.0);
    const double swapTotalGb = static_cast<double>(swapTotal) / (1024.0 * 1024.0);
    const int swapPct = (swapTotal > 0)
                        ? static_cast<int>(static_cast<double>(swapUsed) / static_cast<double>(swapTotal) * 100.0)
                        : 0;
    QString swapSub;
    if (swapTotal > 0)
        swapSub = QString("%1/%2 GB (%3%)")
                  .arg(swapUsedGb, 0, 'f', 1)
                  .arg(swapTotalGb, 0, 'f', 1)
                  .arg(swapPct);
    else
        swapSub = tr("Off");
    if (CFG->PerfShowSwap)
    {
        if (auto *item = this->m_sidePanel->GetItemAt(this->m_swapPanelIndex))
            item->Update(swapSub, this->m_provider->SwapUsageHistory());
    }

    if (CFG->PerfShowDisks)
    {
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
    }

    if (CFG->PerfShowGpu)
    {
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
    }

    if (CFG->PerfShowNetwork)
    {
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

void PerformanceWidget::onSidePanelContextMenu(int /*index*/, const QPoint &globalPos)
{
    QMenu menu(this);

    QAction *cpu = menu.addAction(tr("CPU"));
    cpu->setCheckable(true);
    cpu->setChecked(CFG->PerfShowCpu);

    QAction *memory = menu.addAction(tr("Memory"));
    memory->setCheckable(true);
    memory->setChecked(CFG->PerfShowMemory);

    QAction *swap = menu.addAction(tr("Swap"));
    swap->setCheckable(true);
    swap->setChecked(CFG->PerfShowSwap);

    QAction *disks = menu.addAction(tr("Disks"));
    disks->setCheckable(true);
    disks->setChecked(CFG->PerfShowDisks);

    QAction *network = menu.addAction(tr("NICs"));
    network->setCheckable(true);
    network->setChecked(CFG->PerfShowNetwork);

    QAction *gpu = menu.addAction(tr("GPUs"));
    gpu->setCheckable(true);
    gpu->setChecked(CFG->PerfShowGpu);

    QAction *picked = menu.exec(globalPos);
    if (!picked)
        return;

    bool showCpu = CFG->PerfShowCpu;
    bool showMemory = CFG->PerfShowMemory;
    bool showSwap = CFG->PerfShowSwap;
    bool showDisks = CFG->PerfShowDisks;
    bool showNetwork = CFG->PerfShowNetwork;
    bool showGpu = CFG->PerfShowGpu;

    if (picked == cpu)
        showCpu = cpu->isChecked();
    else if (picked == memory)
        showMemory = memory->isChecked();
    else if (picked == swap)
        showSwap = swap->isChecked();
    else if (picked == disks)
        showDisks = disks->isChecked();
    else if (picked == network)
        showNetwork = network->isChecked();
    else if (picked == gpu)
        showGpu = gpu->isChecked();

    if (!this->anyPanelVisibleAfterToggle(showCpu, showMemory, showSwap, showDisks, showNetwork, showGpu))
        return;

    CFG->PerfShowCpu = showCpu;
    CFG->PerfShowMemory = showMemory;
    CFG->PerfShowSwap = showSwap;
    CFG->PerfShowDisks = showDisks;
    CFG->PerfShowNetwork = showNetwork;
    CFG->PerfShowGpu = showGpu;

    this->applyPanelVisibility();
    this->updateSamplingPolicy();
    if (this->m_active)
        this->onProviderUpdated();
}

void PerformanceWidget::applyPanelVisibility()
{
    if (!this->anyPanelVisibleAfterToggle(CFG->PerfShowCpu,
                                          CFG->PerfShowMemory,
                                          CFG->PerfShowSwap,
                                          CFG->PerfShowDisks,
                                          CFG->PerfShowNetwork,
                                          CFG->PerfShowGpu))
    {
        CFG->PerfShowCpu = true;
    }

    this->m_sidePanel->SetItemVisible(this->m_cpuPanelIndex, CFG->PerfShowCpu);
    this->m_sidePanel->SetItemVisible(this->m_memoryPanelIndex, CFG->PerfShowMemory);
    this->m_sidePanel->SetItemVisible(this->m_swapPanelIndex, CFG->PerfShowSwap);

    for (int i = 0; i < this->m_diskItems.size(); ++i)
        this->m_sidePanel->SetItemVisible(this->m_diskPanelStart + i, CFG->PerfShowDisks);
    for (int i = 0; i < this->m_networkItems.size(); ++i)
        this->m_sidePanel->SetItemVisible(this->m_networkPanelStart + i, CFG->PerfShowNetwork);
    for (int i = 0; i < this->m_gpuItems.size(); ++i)
        this->m_sidePanel->SetItemVisible(this->m_gpuPanelStart + i, CFG->PerfShowGpu);

    const int first = this->m_sidePanel->FirstVisibleIndex();
    if (first >= 0 && !this->m_sidePanel->IsItemVisible(this->m_sidePanel->GetCurrentIndex()))
        this->m_sidePanel->SetCurrentIndex(first);
}

void PerformanceWidget::updateSamplingPolicy()
{
    this->m_provider->SetCpuSamplingEnabled(CFG->PerfShowCpu);
    this->m_provider->SetMemorySamplingEnabled(CFG->PerfShowMemory || CFG->PerfShowSwap);
    this->m_provider->SetSwapSamplingEnabled(CFG->PerfShowSwap);
    this->m_provider->SetDiskSamplingEnabled(CFG->PerfShowDisks);
    this->m_provider->SetNetworkSamplingEnabled(CFG->PerfShowNetwork);
    this->m_provider->SetGpuSamplingEnabled(CFG->PerfShowGpu);
    this->m_provider->SetProcessStatsEnabled(CFG->PerfShowCpu
                                             && this->m_sidePanel->GetCurrentIndex() == this->m_cpuPanelIndex);
}

bool PerformanceWidget::anyPanelVisibleAfterToggle(bool cpu,
                                                   bool memory,
                                                   bool swap,
                                                   bool disks,
                                                   bool network,
                                                   bool gpu) const
{
    return cpu || memory || swap || disks || network || gpu;
}
