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

#include "sidepanelorderdialog.h"
#include "ui_sidepanelorderdialog.h"

#include <QDialogButtonBox>
#include <QListWidget>
#include <QPushButton>

namespace
{
}

SidePanelOrderDialog::SidePanelOrderDialog(const QList<Perf::SidePanelGroup> &groupOrder, QWidget *parent) : QDialog(parent), ui(new Ui::SidePanelOrderDialog)
{
    this->ui->setupUi(this);

    this->m_upButton = this->ui->moveUpButton;
    this->m_downButton = this->ui->moveDownButton;

    for (Perf::SidePanelGroup group : groupOrder)
    {
        auto *item = new QListWidgetItem(Perf::SidePanelGroupLabel(group), this->ui->groupList);
        item->setData(Qt::UserRole, Perf::SidePanelGroupId(group));
    }

    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(this->ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(this->ui->moveUpButton, &QPushButton::clicked, this, [this]() { this->moveCurrentRow(-1); });
    connect(this->ui->moveDownButton, &QPushButton::clicked, this, [this]() { this->moveCurrentRow(1); });
    connect(this->ui->groupList, &QListWidget::currentRowChanged, this, [this]() { this->updateButtons(); });

    if (this->ui->groupList->count() > 0)
        this->ui->groupList->setCurrentRow(0);
    this->updateButtons();
}

SidePanelOrderDialog::~SidePanelOrderDialog()
{
    delete this->ui;
}

QList<Perf::SidePanelGroup> SidePanelOrderDialog::GetOrder() const
{
    QList<Perf::SidePanelGroup> order;
    for (int i = 0; i < this->ui->groupList->count(); ++i)
    {
        const QString id = this->ui->groupList->item(i)->data(Qt::UserRole).toString();
        const auto group = Perf::SidePanelGroupFromId(id);
        if (group.has_value())
            order.append(*group);
    }
    return order;
}

void SidePanelOrderDialog::moveCurrentRow(int delta)
{
    const int row = this->ui->groupList->currentRow();
    const int nextRow = row + delta;
    if (row < 0 || nextRow < 0 || nextRow >= this->ui->groupList->count())
        return;

    QListWidgetItem *item = this->ui->groupList->takeItem(row);
    this->ui->groupList->insertItem(nextRow, item);
    this->ui->groupList->setCurrentRow(nextRow);
}

void SidePanelOrderDialog::updateButtons()
{
    const int row = this->ui->groupList->currentRow();
    this->m_upButton->setEnabled(row > 0);
    this->m_downButton->setEnabled(row >= 0 && row < this->ui->groupList->count() - 1);
}
