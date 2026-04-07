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

#ifndef PROCESSESWIDGET_H
#define PROCESSESWIDGET_H

#include "os/processmodel.h"
#include "os/processfilterproxy.h"

#include <QTimer>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class ProcessesWidget;
}
QT_END_NAMESPACE

class ProcessesWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit ProcessesWidget(QWidget *parent = nullptr);
        ~ProcessesWidget();
        void SetActive(bool active);
        bool IsActive() const { return this->m_active; }

    private slots:
        void onTimerTick();
        void onRefreshRateChanged(int comboIndex);
        void onHeaderContextMenu(const QPoint &pos);
        void onTableContextMenu(const QPoint &pos);
        void updateStatusBar();

    private:
        Ui::ProcessesWidget      *ui;
        OS::ProcessModel         *m_model;
        OS::ProcessFilterProxy   *m_proxy;
        QTimer                   *m_refreshTimer;
        bool                      m_active { false };
        bool                      m_tableContextMenuOpen { false };

        void setupTable();
        void setupRefreshCombo();

        /// Collect PIDs of all currently selected rows.
        QList<pid_t> selectedPids() const;

        /// Send signal to all selected processes; show error dialog on failure.
        void sendSignalToSelected(int signal);

        /// Open renice dialog for all selected processes.
        void reniceSelected();
};

#endif // PROCESSESWIDGET_H
