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

#include "userswidget.h"
#include "ui_userswidget.h"

#include "configuration.h"
#include "misc.h"
#include "os/proc.h"
#include "ui/uihelper.h"

#include <QHeaderView>
#include <QMenu>
#include <QScrollBar>
#include <QTreeWidgetItem>

#include <unistd.h>

namespace
{
    struct UserAgg
    {
        QString name;
        QList<OS::Process> procs;
        double cpuPct { 0.0 };
        quint64 memKb { 0 };
    };

    struct TreeSelectionSnapshot
    {
        QStringList selectedKeys;
        QString currentKey;
        int scrollPos { 0 };
    };

    QString itemKey(const QTreeWidgetItem *item)
    {
        if (!item)
            return {};

        if (item->parent())
            return QStringLiteral("p:%1").arg(item->data(0, Qt::UserRole).toLongLong());

        return QStringLiteral("u:%1").arg(item->data(0, Qt::UserRole).toUInt());
    }
} // namespace

UsersWidget::UsersWidget(QWidget *parent) : QWidget(parent), ui(new Ui::UsersWidget), m_refreshTimer(new QTimer(this))
{
    this->ui->setupUi(this);

    this->m_numCpus = static_cast<int>(::sysconf(_SC_NPROCESSORS_ONLN));
    if (this->m_numCpus < 1)
        this->m_numCpus = 1;

    this->ui->treeWidget->setColumnCount(3);
    this->ui->treeWidget->setHeaderLabels({ tr("User / Process"), tr("CPU"), tr("Memory") });
    this->ui->treeWidget->setRootIsDecorated(true);
    this->ui->treeWidget->setAlternatingRowColors(true);
    this->ui->treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    QHeaderView *hv = this->ui->treeWidget->header();
    hv->setSectionResizeMode(0, QHeaderView::Stretch);
    hv->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    hv->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    connect(this->m_refreshTimer, &QTimer::timeout, this, &UsersWidget::onTimerTick);
    connect(this->ui->treeWidget, &QTreeWidget::customContextMenuRequested, this, &UsersWidget::onContextMenu);
}

UsersWidget::~UsersWidget()
{
    delete this->ui;
}

void UsersWidget::SetActive(bool active)
{
    if (this->m_active == active)
        return;

    this->m_active = active;
    if (active)
    {
        this->onTimerTick();
        this->m_refreshTimer->start(CFG->RefreshRateMs);
    } else
    {
        this->m_refreshTimer->stop();
    }
}

void UsersWidget::onTimerTick()
{
    if (CFG->RefreshPaused)
        return;

    const quint64 totalJiffies = OS::Proc::ReadTotalCpuJiffies();
    const quint64 periodJiffies = (this->m_prevCpuTotalTicks > 0 && totalJiffies > this->m_prevCpuTotalTicks)
                                  ? (totalJiffies - this->m_prevCpuTotalTicks)
                                  : 0;

    OS::Process::LoadOptions opts;
    opts.IncludeKernelTasks = false;
    opts.IncludeOtherUsers = true;
    opts.MyUID = ::getuid();

    QList<OS::Process> fresh = OS::Process::LoadAll(opts);

    if (periodJiffies > 0)
    {
        const double periodPerCpu = static_cast<double>(periodJiffies) / this->m_numCpus;
        for (OS::Process &proc : fresh)
        {
            const auto it = this->m_prevTicks.constFind(proc.PID);
            if (it == this->m_prevTicks.cend() || proc.CPUTicks < it.value())
                continue;

            const double pct = static_cast<double>(proc.CPUTicks - it.value()) / periodPerCpu * 100.0;
            proc.CPUPercent = qMin(pct, 100.0 * this->m_numCpus);
        }
    }

    this->m_prevTicks.clear();
    for (const OS::Process &proc : fresh)
        this->m_prevTicks.insert(proc.PID, proc.CPUTicks);
    this->m_prevCpuTotalTicks = totalJiffies;

    this->rebuildTree(fresh);
}

void UsersWidget::onContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    QHash<QAction *, int> refreshIntervalActions;
    QAction *pausedRefreshAction = nullptr;

    const QTreeWidgetItem *item = this->ui->treeWidget->itemAt(pos);
    const bool isProcessItem = item && item->parent();
    const pid_t selectedPid = isProcessItem
                              ? static_cast<pid_t>(item->data(0, Qt::UserRole).toLongLong())
                              : 0;

    if (isProcessItem && selectedPid > 0)
    {
        QAction *goToProcessAction = menu.addAction(tr("Go to process"));
        connect(goToProcessAction, &QAction::triggered, this, [this, selectedPid]()
        {
            emit goToProcessRequested(selectedPid);
        });
        menu.addSeparator();
    }

    QMenu *refreshMenu = menu.addMenu(tr("Refresh interval"));
    UIHelper::PopulateRefreshIntervalMenu(refreshMenu, refreshIntervalActions, pausedRefreshAction);

    QAction *picked = menu.exec(this->ui->treeWidget->viewport()->mapToGlobal(pos));
    UIHelper::ApplyRefreshIntervalAction(picked, refreshIntervalActions, pausedRefreshAction, this->m_refreshTimer, this->m_active);
}

void UsersWidget::rebuildTree(const QList<OS::Process> &allProcs)
{
    TreeSelectionSnapshot selectionSnapshot;

    // Preserve expanded/collapsed state of top-level user rows.
    if (this->ui->treeWidget->topLevelItemCount() > 0)
    {
        QSet<uid_t> expanded;
        for (int i = 0; i < this->ui->treeWidget->topLevelItemCount(); ++i)
        {
            QTreeWidgetItem *item = this->ui->treeWidget->topLevelItem(i);
            if (!item || !item->isExpanded())
                continue;
            const uid_t uid = static_cast<uid_t>(item->data(0, Qt::UserRole).toUInt());
            expanded.insert(uid);
        }
        this->m_expandedUsers = expanded;
        this->m_hasExpansionSnapshot = true;
    }

    const QList<QTreeWidgetItem *> selectedItems = this->ui->treeWidget->selectedItems();
    selectionSnapshot.selectedKeys.reserve(selectedItems.size());
    for (const QTreeWidgetItem *item : selectedItems)
    {
        const QString key = itemKey(item);
        if (!key.isEmpty())
            selectionSnapshot.selectedKeys.append(key);
    }
    selectionSnapshot.currentKey = itemKey(this->ui->treeWidget->currentItem());
    if (QScrollBar *scrollBar = this->ui->treeWidget->verticalScrollBar())
        selectionSnapshot.scrollPos = scrollBar->value();

    QHash<uid_t, UserAgg> agg;
    for (const OS::Process &p : allProcs)
    {
        if (p.UID < 1000)
            continue;

        auto it = agg.find(p.UID);
        if (it == agg.end())
        {
            UserAgg a;
            a.name = p.User;
            it = agg.insert(p.UID, a);
        }

        it->procs.append(p);
        it->cpuPct += p.CPUPercent;
        it->memKb += p.VMRssKb;
    }

    this->ui->treeWidget->clear();
    QTreeWidgetItem *restoredCurrentItem = nullptr;

    QList<uid_t> uids = agg.keys();
    std::sort(uids.begin(), uids.end(), [&](uid_t a, uid_t b)
    {
        return agg.value(a).cpuPct > agg.value(b).cpuPct;
    });

    for (uid_t uid : uids)
    {
        const UserAgg a = agg.value(uid);
        auto *userItem = new QTreeWidgetItem(this->ui->treeWidget);
        userItem->setText(0, tr("%1 (%2)").arg(a.name).arg(a.procs.size()));
        userItem->setData(0, Qt::UserRole, static_cast<uint>(uid));
        userItem->setText(1, QString::number(a.cpuPct, 'f', 1) + "%");
        userItem->setText(2, Misc::FormatKiB(a.memKb, 1));
        userItem->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        userItem->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        const QString userKey = itemKey(userItem);
        if (selectionSnapshot.selectedKeys.contains(userKey))
            userItem->setSelected(true);
        if (!restoredCurrentItem && selectionSnapshot.currentKey == userKey)
            restoredCurrentItem = userItem;

        QList<OS::Process> procs = a.procs;
        std::sort(procs.begin(), procs.end(), [](const OS::Process &x, const OS::Process &y)
        {
            return x.CPUPercent > y.CPUPercent;
        });

        for (const OS::Process &p : procs)
        {
            auto *procItem = new QTreeWidgetItem(userItem);
            procItem->setText(0, tr("%1 (pid %2)").arg(p.Name).arg(p.PID));
            procItem->setData(0, Qt::UserRole, static_cast<qlonglong>(p.PID));
            procItem->setText(1, QString::number(p.CPUPercent, 'f', 1) + "%");
            procItem->setText(2, Misc::FormatKiB(p.VMRssKb, 1));
            procItem->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
            procItem->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
            const QString procKey = itemKey(procItem);
            if (selectionSnapshot.selectedKeys.contains(procKey))
                procItem->setSelected(true);
            if (!restoredCurrentItem && selectionSnapshot.currentKey == procKey)
                restoredCurrentItem = procItem;
        }

        userItem->setExpanded(this->m_hasExpansionSnapshot && this->m_expandedUsers.contains(uid));
    }

    if (restoredCurrentItem)
    {
        this->ui->treeWidget->setCurrentItem(restoredCurrentItem);
        this->ui->treeWidget->scrollToItem(restoredCurrentItem, QAbstractItemView::PositionAtCenter);
    } else if (QScrollBar *scrollBar = this->ui->treeWidget->verticalScrollBar())
    {
        scrollBar->setValue(selectionSnapshot.scrollPos);
    }

    this->ui->statusLabel->setText(tr("Logged in users: %1").arg(agg.size()));
}
