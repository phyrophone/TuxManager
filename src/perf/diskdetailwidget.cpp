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

#include "diskdetailwidget.h"
#include "globals.h"
#include "metrics.h"
#include "ui_diskdetailwidget.h"
#include "../colorscheme.h"
#include "../misc.h"
#include "../ui/widgetstyle.h"

#include <QGridLayout>
#include <QLabel>

using namespace Perf;

DiskDetailWidget::DiskDetailWidget(QWidget *parent) : QWidget(parent), ui(new Ui::DiskDetailWidget)
{
    this->ui->setupUi(this);
    const ColorScheme *scheme = ColorScheme::GetCurrent();

    WidgetStyle::ApplyTextStyle(this->ui->titleLabel, scheme->DiskTitleColor, 18, true);
    WidgetStyle::ApplyTextStyle(this->ui->modelLabel, scheme->DiskHeaderValueColor, 12);
    WidgetStyle::ApplyTextStyle(this->ui->activeGraphLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->activeGraphMaxLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->transferGraphLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->transferGraphMaxLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->activeTimeLeftLabel, scheme->AxisLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->activeTimeRightLabel, scheme->AxisLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->transferTimeLeftLabel, scheme->AxisLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->transferTimeRightLabel, scheme->AxisLabelColor, 8);

    if (QGridLayout *statsGrid = this->findChild<QGridLayout *>("statsGrid"))
    {
        for (int row = 0; row < statsGrid->rowCount(); ++row)
        {
            for (int column = 0; column < statsGrid->columnCount(); column += 2)
            {
                if (QLayoutItem *item = statsGrid->itemAtPosition(row, column))
                {
                    if (QLabel *label = qobject_cast<QLabel *>(item->widget()))
                        WidgetStyle::ApplyTextStyle(label, scheme->StatLabelColor);
                }
            }
        }
    }

    // Active time graph
    this->ui->activeGraphWidget->SetColor(scheme->DiskGraphLineColor, scheme->DiskGraphFillColor);
    this->ui->activeGraphWidget->SetSampleCapacity(TUX_MANAGER_HISTORY_SIZE);
    this->ui->activeGraphWidget->SetGridColumns(6);
    this->ui->activeGraphWidget->SetGridRows(4);
    this->ui->activeGraphWidget->SetSeriesNames(tr("Active time"));
    this->ui->activeGraphWidget->SetValueFormat(GraphWidget::ValueFormat::Percent);

    // Transfer graph (read + write overlay)
    this->ui->transferGraphWidget->SetColor(scheme->DiskTransferGraphLineColor, scheme->DiskTransferGraphFillColor, scheme->DiskTransferGraphSecondaryFillColor);
    this->ui->transferGraphWidget->SetSampleCapacity(TUX_MANAGER_HISTORY_SIZE);
    this->ui->transferGraphWidget->SetGridColumns(6);
    this->ui->transferGraphWidget->SetGridRows(4);
    this->ui->transferGraphWidget->SetSeriesNames(tr("Read"), tr("Write"));
    this->ui->transferGraphWidget->SetValueFormat(GraphWidget::ValueFormat::BytesPerSec);
}

DiskDetailWidget::~DiskDetailWidget()
{
    delete this->ui;
}

void DiskDetailWidget::ApplyColorScheme()
{
    const ColorScheme *scheme = ColorScheme::GetCurrent();
    WidgetStyle::ApplyTextStyle(this->ui->titleLabel, scheme->DiskTitleColor, 18, true);
    WidgetStyle::ApplyTextStyle(this->ui->modelLabel, scheme->DiskHeaderValueColor, 12);
    WidgetStyle::ApplyTextStyle(this->ui->activeGraphLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->activeGraphMaxLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->transferGraphLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->transferGraphMaxLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->activeTimeLeftLabel, scheme->AxisLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->activeTimeRightLabel, scheme->AxisLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->transferTimeLeftLabel, scheme->AxisLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->transferTimeRightLabel, scheme->AxisLabelColor, 8);

    if (QGridLayout *statsGrid = this->findChild<QGridLayout *>("statsGrid"))
    {
        for (int row = 0; row < statsGrid->rowCount(); ++row)
        {
            for (int column = 0; column < statsGrid->columnCount(); column += 2)
            {
                if (QLayoutItem *item = statsGrid->itemAtPosition(row, column))
                {
                    if (QLabel *label = qobject_cast<QLabel *>(item->widget()))
                        WidgetStyle::ApplyTextStyle(label, scheme->StatLabelColor);
                }
            }
        }
    }

    this->ui->activeGraphWidget->SetColor(scheme->DiskGraphLineColor, scheme->DiskGraphFillColor);
    this->ui->transferGraphWidget->SetColor(scheme->DiskTransferGraphLineColor, scheme->DiskTransferGraphFillColor, scheme->DiskTransferGraphSecondaryFillColor);
    this->update();
}

void DiskDetailWidget::SetDisk(Metrics *provider, int index)
{
    if (this->m_provider)
        disconnect(this->m_provider, &Metrics::updated, this, &DiskDetailWidget::onUpdated);

    this->m_provider = provider;
    this->m_diskIndex = index;
    this->m_activeHistory = nullptr;
    this->m_readHistory = nullptr;
    this->m_writeHistory = nullptr;

    if (this->m_provider)
    {
        if (this->m_diskIndex >= 0 && this->m_diskIndex < Metrics::GetStorage()->DiskCount())
        {
            const QString name = Metrics::GetStorage()->DiskName(this->m_diskIndex);
            const QString model = Metrics::GetStorage()->DiskModel(this->m_diskIndex);
            const QString type = Metrics::GetStorage()->DiskType(this->m_diskIndex);

            this->m_activeHistory = &Metrics::GetStorage()->DiskActiveHistory(this->m_diskIndex);
            this->m_readHistory = &Metrics::GetStorage()->DiskReadHistory(this->m_diskIndex);
            this->m_writeHistory = &Metrics::GetStorage()->DiskWriteHistory(this->m_diskIndex);

            this->ui->titleLabel->setText(tr("Disk (%1)").arg(name));
            this->ui->modelLabel->setText(model);
            this->ui->typeValueLabel->setText(type);
            this->ui->deviceValueLabel->setText("/dev/" + name);

            this->ui->activeGraphWidget->SetDataSource(*this->m_activeHistory, 100.0);
            this->ui->transferGraphWidget->SetDataSource(*this->m_readHistory, 1024.0);
            this->ui->transferGraphWidget->SetOverlayDataSource(*this->m_writeHistory);
        }
        connect(this->m_provider, &Metrics::updated, this, &DiskDetailWidget::onUpdated);
    }
    this->onUpdated();
}

void DiskDetailWidget::onUpdated()
{
    if (!this->m_provider || !this->m_activeHistory || !this->m_readHistory || !this->m_writeHistory
            || this->m_diskIndex < 0 || this->m_diskIndex >= Metrics::GetStorage()->DiskCount())
        return;

    const double active = Metrics::GetStorage()->DiskActivePercent(this->m_diskIndex);
    const double readBps = Metrics::GetStorage()->DiskReadBytesPerSec(this->m_diskIndex);
    const double writeBps = Metrics::GetStorage()->DiskWriteBytesPerSec(this->m_diskIndex);
    const qint64 capacityBytes = Metrics::GetStorage()->DiskCapacityBytes(this->m_diskIndex);
    const qint64 formattedBytes = Metrics::GetStorage()->DiskFormattedBytes(this->m_diskIndex);
    const bool isSystemDisk = Metrics::GetStorage()->DiskIsSystemDisk(this->m_diskIndex);
    const bool hasSwapFile = Metrics::GetStorage()->DiskHasSwapFile(this->m_diskIndex);
    this->ui->activeValueLabel->setText(QString::number(active, 'f', 0) + "%");
    this->ui->readValueLabel->setText(Misc::FormatBytesPerSecond(readBps));
    this->ui->writeValueLabel->setText(Misc::FormatBytesPerSecond(writeBps));
    this->ui->capacityValueLabel->setText(Misc::FormatBytes(static_cast<quint64>(qMax<qint64>(0, capacityBytes)), 1));
    this->ui->formattedValueLabel->setText(formattedBytes > 0
                                           ? Misc::FormatBytes(static_cast<quint64>(qMax<qint64>(0, formattedBytes)), 1)
                                           : tr("-"));
    this->ui->systemDiskValueLabel->setText(isSystemDisk ? tr("Yes") : tr("No"));
    this->ui->pageFileValueLabel->setText(hasSwapFile ? tr("Yes") : tr("No"));

    this->ui->activeGraphMaxLabel->setText(tr("100%"));

    const double maxRate = Metrics::GetStorage()->DiskMaxTransferBytesPerSec(this->m_diskIndex);
    this->ui->transferGraphWidget->SetMax(maxRate);
    this->ui->activeGraphWidget->Tick();
    this->ui->transferGraphWidget->Tick();
    this->ui->transferGraphMaxLabel->setText(Misc::FormatBytesPerSecond(maxRate));
}
