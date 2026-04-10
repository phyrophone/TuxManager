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

#include "networkdetailwidget.h"
#include "globals.h"
#include "metrics.h"
#include "ui_networkdetailwidget.h"
#include "../colorscheme.h"
#include "../misc.h"
#include "../ui/widgetstyle.h"

#include <QGridLayout>
#include <QLabel>

using namespace Perf;

NetworkDetailWidget::NetworkDetailWidget(QWidget *parent) : QWidget(parent), ui(new Ui::NetworkDetailWidget)
{
    this->ui->setupUi(this);
    const ColorScheme *scheme = ColorScheme::GetCurrent();

    WidgetStyle::ApplyTextStyle(this->ui->titleLabel, scheme->NetworkTitleColor, 18, true);
    WidgetStyle::ApplyTextStyle(this->ui->throughputGraphMaxLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->throughputLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->timeLeftLabel, scheme->AxisLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->timeRightLabel, scheme->AxisLabelColor, 8);

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

    this->ui->throughputGraphWidget->SetColor(scheme->NetworkGraphLineColor, scheme->NetworkGraphFillColor, scheme->NetworkGraphSecondaryFillColor);
    this->ui->throughputGraphWidget->SetSampleCapacity(TUX_MANAGER_HISTORY_SIZE);
    this->ui->throughputGraphWidget->SetGridColumns(6);
    this->ui->throughputGraphWidget->SetGridRows(4);
    this->ui->throughputGraphWidget->SetSeriesNames(tr("Receive"), tr("Send"));
    this->ui->throughputGraphWidget->SetValueFormat(GraphWidget::ValueFormat::BytesPerSec);
}

NetworkDetailWidget::~NetworkDetailWidget()
{
    delete this->ui;
}

void NetworkDetailWidget::ApplyColorScheme()
{
    const ColorScheme *scheme = ColorScheme::GetCurrent();
    WidgetStyle::ApplyTextStyle(this->ui->titleLabel, scheme->NetworkTitleColor, 18, true);
    WidgetStyle::ApplyTextStyle(this->ui->throughputGraphMaxLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->throughputLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->timeLeftLabel, scheme->AxisLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->timeRightLabel, scheme->AxisLabelColor, 8);

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

    this->ui->throughputGraphWidget->SetColor(scheme->NetworkGraphLineColor, scheme->NetworkGraphFillColor, scheme->NetworkGraphSecondaryFillColor);
    this->update();
}

void NetworkDetailWidget::SetNetwork(int index)
{
    this->m_networkIndex = index;

    if (this->m_networkIndex >= 0 && this->m_networkIndex < Metrics::GetNetwork()->NetworkCount())
    {
        const Network::NetworkInfo &network = Metrics::GetNetwork()->FromIndex(this->m_networkIndex);

        this->ui->titleLabel->setText(tr("NIC (%1)").arg(network.Name));
        this->ui->adapterValueLabel->setText(network.Name);
        this->ui->typeValueLabel->setText(network.Type);
        this->ui->speedValueLabel->setText(network.LinkSpeedMbps > 0 ? QString::number(network.LinkSpeedMbps) + tr(" Mbps") : tr("Unknown"));
        this->ui->ipv4ValueLabel->setText(network.IPv4.isEmpty() ? tr("—") : network.IPv4);
        this->ui->ipv6ValueLabel->setText(network.IPv6.isEmpty() ? tr("—") : network.IPv6);

        this->ui->throughputGraphWidget->SetDataSource(network.RxHistory, 1024.0);
        this->ui->throughputGraphWidget->SetOverlayDataSource(network.TxHistory);
    }
    connect(Metrics::Get(), &Metrics::updated, this, &NetworkDetailWidget::onUpdated);
    this->onUpdated();
}

void NetworkDetailWidget::onUpdated()
{
    if (this->m_networkIndex < 0 || this->m_networkIndex >= Metrics::GetNetwork()->NetworkCount())
        return;

    const Network::NetworkInfo &network = Metrics::GetNetwork()->FromIndex(this->m_networkIndex);
    this->ui->sendValueLabel->setText(Misc::FormatBytesPerSecond(network.TxBps));
    this->ui->receiveValueLabel->setText(Misc::FormatBytesPerSecond(network.RxBps));

    this->ui->throughputGraphWidget->SetMax(network.MaxThroughputBps);
    this->ui->throughputGraphWidget->Tick();
    this->ui->throughputGraphMaxLabel->setText(Misc::FormatBytesPerSecond(network.MaxThroughputBps));
}
