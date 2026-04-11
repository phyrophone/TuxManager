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

#include "swapdetailwidget.h"
#include "../colorscheme.h"
#include "../misc.h"
#include "../ui/widgetstyle.h"
#include "configuration.h"
#include "globals.h"
#include "metrics.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QVBoxLayout>
#include <utility>

using namespace Perf;

SwapDetailWidget::SwapDetailWidget(QWidget *parent) : QWidget(parent)
{
    const ColorScheme *scheme = ColorScheme::GetCurrent();
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 10);
    root->setSpacing(6);

    auto *header = new QHBoxLayout();
    this->m_titleLabel = new QLabel(tr("Swap"), this);
    QFont titleFont = this->m_titleLabel->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    this->m_titleLabel->setFont(titleFont);
    WidgetStyle::ApplyTextStyle(this->m_titleLabel, scheme->SwapUsageGraphLineColor, 18, true);

    this->m_totalLabel = new QLabel(tr("0 GB"), this);
    this->m_totalLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFont totalFont = this->m_totalLabel->font();
    totalFont.setPointSize(11);
    this->m_totalLabel->setFont(totalFont);
    WidgetStyle::ApplyTextStyle(this->m_totalLabel, scheme->MutedTextColor, 11);

    header->addWidget(this->m_titleLabel, 1);
    header->addWidget(this->m_totalLabel, 1);
    root->addLayout(header);

    auto *usageHeader = new QHBoxLayout();
    QLabel *usageLabel = new QLabel(tr("Swap usage"), this);
    this->m_statLabels.append(usageLabel);
    usageHeader->addWidget(usageLabel, 1);
    this->m_usageValueLabel = new QLabel(tr("0%"), this);
    this->m_usageValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    usageHeader->addWidget(this->m_usageValueLabel);
    root->addLayout(usageHeader);

    this->m_usageGraphArea = new SwapGraphArea(this);
    this->m_usageGraphArea->setMinimumHeight(220);
    root->addWidget(this->m_usageGraphArea, 1);

    auto *usageTimeAxis = new QHBoxLayout();
    QLabel *usageTimeLeft = new QLabel(tr("60 seconds"), this);
    this->m_axisLabels.append(usageTimeLeft);
    usageTimeAxis->addWidget(usageTimeLeft);
    usageTimeAxis->addStretch(1);
    QLabel *usageTimeRight = new QLabel(tr("0"), this);
    this->m_axisLabels.append(usageTimeRight);
    usageTimeAxis->addWidget(usageTimeRight);
    root->addLayout(usageTimeAxis);

    auto *activityHeader = new QHBoxLayout();
    QLabel *activityLabel = new QLabel(tr("Swap activity"), this);
    this->m_statLabels.append(activityLabel);
    activityHeader->addWidget(activityLabel, 1);
    this->m_activityMaxLabel = new QLabel(tr("0 KB/s"), this);
    this->m_activityMaxLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    activityHeader->addWidget(this->m_activityMaxLabel);
    root->addLayout(activityHeader);

    this->m_activityGraph = new GraphWidget(this);
    this->m_activityGraph->SetColor(scheme->SwapActivityGraphLineColor, scheme->SwapActivityGraphFillColor, scheme->SwapActivityGraphSecondaryFillColor);
    this->m_activityGraph->SetSampleCapacity(TUX_MANAGER_HISTORY_SIZE);
    this->m_activityGraph->SetGridColumns(6);
    this->m_activityGraph->SetGridRows(4);
    this->m_activityGraph->SetSeriesNames(tr("Swap in"), tr("Swap out"));
    this->m_activityGraph->SetValueFormat(GraphWidget::ValueFormat::BytesPerSec);
    this->m_activityGraph->setToolTip(
                tr("Swap in: disk -> RAM (pages read back into memory)\n"
                   "Swap out: RAM -> disk (pages written to swap storage)"));
    this->m_activityGraph->setMinimumHeight(110);
    root->addWidget(this->m_activityGraph);

    auto *activityTimeAxis = new QHBoxLayout();
    QLabel *activityTimeLeft = new QLabel(tr("60 seconds"), this);
    this->m_axisLabels.append(activityTimeLeft);
    activityTimeAxis->addWidget(activityTimeLeft);
    activityTimeAxis->addStretch(1);
    QLabel *activityTimeRight = new QLabel(tr("0"), this);
    this->m_axisLabels.append(activityTimeRight);
    activityTimeAxis->addWidget(activityTimeRight);
    root->addLayout(activityTimeAxis);

    auto *stats = new QGridLayout();
    stats->setHorizontalSpacing(24);
    stats->setVerticalSpacing(8);

    auto mk = [this](const QString &txt)
    {
        auto *l = new QLabel(txt, this);
        WidgetStyle::ApplyTextStyle(l, ColorScheme::GetCurrent()->StatLabelColor);
        this->m_statLabels.append(l);
        return l;
    };

    this->m_inUseValueLabel = new QLabel(tr("0 GB"), this);
    this->m_freeValueLabel = new QLabel(tr("0 GB"), this);
    this->m_inRateValueLabel = new QLabel(tr("0 KB/s"), this);
    this->m_outRateValueLabel = new QLabel(tr("0 KB/s"), this);
    this->m_inRateValueLabel->setToolTip(tr("Swap in direction: disk -> RAM"));
    this->m_outRateValueLabel->setToolTip(tr("Swap out direction: RAM -> disk"));

    stats->addWidget(mk(tr("In use")), 0, 0);
    stats->addWidget(this->m_inUseValueLabel, 0, 1);
    stats->addWidget(mk(tr("Free")), 0, 2);
    stats->addWidget(this->m_freeValueLabel, 0, 3);
    QLabel *swapInLabel = mk(tr("Swap in"));
    swapInLabel->setToolTip(tr("Swap in direction: disk -> RAM"));
    stats->addWidget(swapInLabel, 1, 0);
    stats->addWidget(this->m_inRateValueLabel, 1, 1);
    QLabel *swapOutLabel = mk(tr("Swap out"));
    swapOutLabel->setToolTip(tr("Swap out direction: RAM -> disk"));
    stats->addWidget(swapOutLabel, 1, 2);
    stats->addWidget(this->m_outRateValueLabel, 1, 3);
    root->addLayout(stats);
}

void SwapDetailWidget::Init()
{
    this->m_outHistory = &Metrics::GetSwap()->SwapOutHistory();
    this->m_inHistory = &Metrics::GetSwap()->SwapInHistory();

    connect(this->m_usageGraphArea, &SwapGraphArea::contextMenuRequested, this, &SwapDetailWidget::onContextMenuRequested);

    this->m_usageGraphArea->SetMode(
                CFG->SwapGraphMode == 1
                ? SwapGraphArea::GraphMode::PerDevice
                : SwapGraphArea::GraphMode::Overall);
    this->m_usageGraphArea->Init();
    this->m_activityGraph->SetDataSource(*this->m_inHistory, 1024.0);
    this->m_activityGraph->SetOverlayDataSource(*this->m_outHistory);

    connect(Metrics::Get(), &Metrics::swapDevicesChanged, this, &SwapDetailWidget::onSwapDevicesChanged);
    connect(Metrics::Get(), &Metrics::updated, this, &SwapDetailWidget::onUpdated);
    this->onUpdated();
}

void SwapDetailWidget::ApplyColorScheme()
{
    const ColorScheme *scheme = ColorScheme::GetCurrent();
    WidgetStyle::ApplyTextStyle(this->m_titleLabel, scheme->SwapUsageGraphLineColor, 18, true);
    WidgetStyle::ApplyTextStyle(this->m_totalLabel, scheme->MutedTextColor, 11);
    for (QLabel *label : std::as_const(this->m_statLabels))
        WidgetStyle::ApplyTextStyle(label, scheme->StatLabelColor);
    for (QLabel *label : std::as_const(this->m_axisLabels))
        WidgetStyle::ApplyTextStyle(label, scheme->AxisLabelColor);
    this->m_usageGraphArea->ApplyColorScheme();
    this->m_activityGraph->SetColor(scheme->SwapActivityGraphLineColor, scheme->SwapActivityGraphFillColor, scheme->SwapActivityGraphSecondaryFillColor);
    this->update();
}

void SwapDetailWidget::onUpdated()
{
    const qint64 totalKb = Metrics::GetSwap()->SwapTotalKb();
    const qint64 usedKb = Metrics::GetSwap()->SwapUsedKb();
    const qint64 freeKb = Metrics::GetSwap()->SwapFreeKb();
    const double inBps = Metrics::GetSwap()->SwapInBytesPerSec();
    const double outBps = Metrics::GetSwap()->SwapOutBytesPerSec();

    const double usedPct = (totalKb > 0)
                           ? static_cast<double>(usedKb) * 100.0 / static_cast<double>(totalKb)
                           : 0.0;

    this->m_totalLabel->setText(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, totalKb)), 1));
    this->m_usageValueLabel->setText(QString::number(usedPct, 'f', 0) + "%");

    this->m_inUseValueLabel->setText(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, usedKb)), 1));
    this->m_freeValueLabel->setText(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, freeKb)), 1));
    this->m_inRateValueLabel->setText(Misc::FormatBytesPerSecond(inBps));
    this->m_outRateValueLabel->setText(Misc::FormatBytesPerSecond(outBps));

    const double maxRate = Metrics::GetSwap()->SwapMaxActivityBytesPerSec();
    this->m_activityGraph->SetMax(maxRate);

    this->m_usageGraphArea->UpdateData();
    this->m_activityGraph->Tick();
    this->m_activityMaxLabel->setText(Misc::FormatBytesPerSecond(maxRate));
}

void SwapDetailWidget::onContextMenuRequested(const QPoint &globalPos)
{
    QMenu menu(this);
    menu.setTitle(tr("Swap graph options"));

    QMenu *graphMenu = menu.addMenu(tr("Change graph to"));

    QAction *actOverall = graphMenu->addAction(tr("Overall usage"));
    QAction *actPerDevice = graphMenu->addAction(tr("Swap devices"));
    actOverall->setCheckable(true);
    actPerDevice->setCheckable(true);

    const bool isOverall = (this->m_usageGraphArea->GetMode() == SwapGraphArea::GraphMode::Overall);
    actOverall->setChecked(isOverall);
    actPerDevice->setChecked(!isOverall);

    connect(actOverall, &QAction::triggered, this, [this]()
    {
        this->m_usageGraphArea->SetMode(SwapGraphArea::GraphMode::Overall);
        CFG->SwapGraphMode = 0;
    });
    connect(actPerDevice, &QAction::triggered, this, [this]()
    {
        this->m_usageGraphArea->SetMode(SwapGraphArea::GraphMode::PerDevice);
        CFG->SwapGraphMode = 1;
    });

    menu.addSeparator();

    QAction *actCopy = menu.addAction(tr("Copy\tCtrl+C"));
    connect(actCopy, &QAction::triggered, this, [this]()
    {
        QApplication::clipboard()->setPixmap(this->m_usageGraphArea->grab());
    });

    menu.exec(globalPos);
}

void SwapDetailWidget::onSwapDevicesChanged()
{
    this->m_usageGraphArea->RebindDevices();
}
