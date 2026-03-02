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
    connect(this->m_provider, &Perf::PerfDataProvider::updated,
            this, &PerformanceWidget::onProviderUpdated);

    // Expensive process/thread counting is only needed for CPU detail page.
    connect(this->m_sidePanel, &Perf::SidePanel::currentChanged,
            this, [this](int index)
    {
        this->m_provider->setProcessStatsEnabled(index == PanelCpu);
    });
    this->m_provider->setProcessStatsEnabled(this->m_sidePanel->currentIndex() == PanelCpu);

    this->setActive(false);

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
    cpuItem->setGraphColor(QColor(0x00, 0xbc, 0xff), QColor(0x00, 0x4c, 0x8a, 120));
    this->m_sidePanel->addItem(cpuItem);
    this->m_stack->addWidget(this->m_cpuDetail);

    // ── Memory item ──────────────────────────────────────────────────────────
    auto *memItem = new Perf::SidePanelItem(tr("Memory"), this);
    memItem->setGraphColor(QColor(0xcc, 0x44, 0xcc), QColor(0x66, 0x11, 0x66, 130));
    this->m_sidePanel->addItem(memItem);
    this->m_stack->addWidget(this->m_memDetail);

    this->setupDiskPanels();
    this->setupGpuPanels();

    // Side-panel selection drives the stacked widget page
    connect(this->m_sidePanel, &Perf::SidePanel::currentChanged,
            this->m_stack, &QStackedWidget::setCurrentIndex);
}

void PerformanceWidget::setupDiskPanels()
{
    const int count = this->m_provider->diskCount();
    for (int i = 0; i < count; ++i)
    {
        const QString devName = this->m_provider->diskName(i);
        this->m_diskNames.append(devName);

        auto *item = new Perf::SidePanelItem(tr("Disk (%1)").arg(devName), this);
        item->setGraphColor(QColor(0x66, 0xbb, 0x44), QColor(0x33, 0x66, 0x22, 120));
        this->m_sidePanel->addItem(item);
        this->m_diskItems.append(item);

        auto *detail = new Perf::DiskDetailWidget(this);
        detail->setProvider(this->m_provider);
        detail->setDiskIndex(i);
        this->m_stack->addWidget(detail);
        this->m_diskDetails.append(detail);
    }
}

void PerformanceWidget::setupGpuPanels()
{
    const int count = this->m_provider->gpuCount();
    for (int i = 0; i < count; ++i)
    {
        const QString gpuName = this->m_provider->gpuName(i);
        this->m_gpuNames.append(gpuName);

        auto *item = new Perf::SidePanelItem(tr("GPU %1").arg(i), this);
        item->setGraphColor(QColor(0x44, 0xa8, 0xff), QColor(0x1e, 0x4d, 0x82, 110));
        this->m_sidePanel->addItem(item);
        this->m_gpuItems.append(item);

        auto *detail = new Perf::GpuDetailWidget(this);
        detail->setProvider(this->m_provider);
        detail->setGpuIndex(i);
        this->m_stack->addWidget(detail);
        this->m_gpuDetails.append(detail);
    }
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void PerformanceWidget::onProviderUpdated()
{
    // Update CPU side panel item
    const double cpuPct = this->m_provider->cpuPercent();
    const QString cpuSub = QString::number(cpuPct, 'f', 0) + "%";
    if (auto *item = this->m_sidePanel->itemAt(PanelCpu))
        item->update(cpuSub, this->m_provider->cpuHistory());

    // Update Memory side panel item
    const qint64 used  = this->m_provider->memUsedKb();
    const qint64 total = this->m_provider->memTotalKb();
    const double usedGb  = static_cast<double>(used)  / (1024.0 * 1024.0);
    const double totalGb = static_cast<double>(total) / (1024.0 * 1024.0);
    const int    pct     = total > 0
                           ? static_cast<int>(static_cast<double>(used) / total * 100.0)
                           : 0;
    const QString memSub = QString("%1/%2 GB (%3%)")
                           .arg(usedGb,  0, 'f', 1)
                           .arg(totalGb, 0, 'f', 1)
                           .arg(pct);
    if (auto *item = this->m_sidePanel->itemAt(PanelMemory))
        item->update(memSub, this->m_provider->memHistory());

    for (int i = 0; i < this->m_diskItems.size(); ++i)
    {
        if (i >= this->m_provider->diskCount())
            break;
        auto *item = this->m_diskItems.at(i);
        if (!item)
            continue;

        const QString diskSub = tr("%1 %2")
                                .arg(this->m_provider->diskType(i))
                                .arg(QString::number(this->m_provider->diskActivePercent(i), 'f', 0) + "%");
        item->update(diskSub, this->m_provider->diskActiveHistory(i));
    }

    for (int i = 0; i < this->m_gpuItems.size(); ++i)
    {
        if (i >= this->m_provider->gpuCount())
            break;
        auto *item = this->m_gpuItems.at(i);
        if (!item)
            continue;

        const QString gpuSub = tr("%1%2")
                               .arg(QString::number(this->m_provider->gpuUtilPercent(i), 'f', 0))
                               .arg("%");
        item->update(gpuSub, this->m_provider->gpuUtilHistory(i));
    }
}

void PerformanceWidget::setActive(bool active)
{
    if (this->m_active == active)
        return;

    this->m_active = active;
    this->m_provider->setActive(active);
    if (active)
        this->onProviderUpdated();
}
