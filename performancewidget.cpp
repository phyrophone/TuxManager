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

    // Side-panel selection drives the stacked widget page
    connect(this->m_sidePanel, &Perf::SidePanel::currentChanged,
            this->m_stack, &QStackedWidget::setCurrentIndex);
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
}

