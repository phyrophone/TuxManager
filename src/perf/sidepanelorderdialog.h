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

#ifndef PERF_SIDEPANELORDERDIALOG_H
#define PERF_SIDEPANELORDERDIALOG_H

#include "sidepanelgroup.h"

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class SidePanelOrderDialog; }
class QPushButton;
QT_END_NAMESPACE

class SidePanelOrderDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit SidePanelOrderDialog(const QList<Perf::SidePanelGroup> &groupOrder, QWidget *parent = nullptr);
        ~SidePanelOrderDialog();

        QList<Perf::SidePanelGroup> GetOrder() const;

    private:
        void moveCurrentRow(int delta);
        void updateButtons();

        Ui::SidePanelOrderDialog *ui { nullptr };
        QPushButton *m_upButton { nullptr };
        QPushButton *m_downButton { nullptr };
};

#endif // PERF_SIDEPANELORDERDIALOG_H
