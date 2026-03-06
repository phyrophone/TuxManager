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

#include <QFile>
#include <QHeaderView>
#include <QMenu>
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
} // namespace

UsersWidget::UsersWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::UsersWidget)
    , m_refreshTimer(new QTimer(this))
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

    connect(this->m_refreshTimer, &QTimer::timeout,
            this, &UsersWidget::onTimerTick);
    connect(this->ui->treeWidget, &QTreeWidget::customContextMenuRequested,
            this, &UsersWidget::onContextMenu);
}

UsersWidget::~UsersWidget()
{
    delete this->ui;
}

void UsersWidget::setActive(bool active)
{
    if (this->m_active == active)
        return;

    this->m_active = active;
    if (active)
    {
        this->onTimerTick();
        this->m_refreshTimer->start(CFG->RefreshRateMs);
    }
    else
    {
        this->m_refreshTimer->stop();
    }
}

void UsersWidget::onTimerTick()
{
    const quint64 totalJiffies = readTotalCpuJiffies();
    const quint64 periodJiffies = (this->m_prevCpuTotalTicks > 0 && totalJiffies > this->m_prevCpuTotalTicks)
                                  ? (totalJiffies - this->m_prevCpuTotalTicks)
                                  : 0;

    OS::Process::LoadOptions opts;
    opts.includeKernelTasks = false;
    opts.includeOtherUsers = true;
    opts.myUid = ::getuid();

    QList<OS::Process> fresh = OS::Process::loadAll(opts);

    if (periodJiffies > 0)
    {
        const double periodPerCpu = static_cast<double>(periodJiffies) / this->m_numCpus;
        for (OS::Process &proc : fresh)
        {
            const auto it = this->m_prevTicks.constFind(proc.pid);
            if (it == this->m_prevTicks.cend() || proc.cpuTicks < it.value())
                continue;

            const double pct = static_cast<double>(proc.cpuTicks - it.value()) / periodPerCpu * 100.0;
            proc.cpuPercent = qMin(pct, 100.0 * this->m_numCpus);
        }
    }

    this->m_prevTicks.clear();
    for (const OS::Process &proc : fresh)
        this->m_prevTicks.insert(proc.pid, proc.cpuTicks);
    this->m_prevCpuTotalTicks = totalJiffies;

    this->rebuildTree(fresh);
}

void UsersWidget::onContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    QMenu *refreshMenu = menu.addMenu(tr("Refresh interval"));

    const struct { int ms; const char *label; } choices[] = {
        { 250,  "250 ms" },
        { 500,  "500 ms" },
        { 1000, "1 second" },
        { 2000, "2 seconds" },
        { 5000, "5 seconds" }
    };

    for (const auto &c : choices)
    {
        QAction *a = refreshMenu->addAction(tr(c.label));
        a->setCheckable(true);
        a->setChecked(CFG->RefreshRateMs == c.ms);
        connect(a, &QAction::triggered, this, [this, c]()
        {
            CFG->RefreshRateMs = c.ms;
            if (this->m_active)
                this->m_refreshTimer->start(CFG->RefreshRateMs);
        });
    }

    menu.exec(this->ui->treeWidget->viewport()->mapToGlobal(pos));
}

void UsersWidget::rebuildTree(const QList<OS::Process> &allProcs)
{
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

    QHash<uid_t, UserAgg> agg;
    for (const OS::Process &p : allProcs)
    {
        if (p.uid < 1000)
            continue;

        auto it = agg.find(p.uid);
        if (it == agg.end())
        {
            UserAgg a;
            a.name = p.user;
            it = agg.insert(p.uid, a);
        }

        it->procs.append(p);
        it->cpuPct += p.cpuPercent;
        it->memKb += p.vmRssKb;
    }

    this->ui->treeWidget->clear();

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
        userItem->setText(2, formatMemory(a.memKb));
        userItem->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        userItem->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);

        QList<OS::Process> procs = a.procs;
        std::sort(procs.begin(), procs.end(), [](const OS::Process &x, const OS::Process &y)
        {
            return x.cpuPercent > y.cpuPercent;
        });

        for (const OS::Process &p : procs)
        {
            auto *procItem = new QTreeWidgetItem(userItem);
            procItem->setText(0, tr("%1 (pid %2)").arg(p.name).arg(p.pid));
            procItem->setText(1, QString::number(p.cpuPercent, 'f', 1) + "%");
            procItem->setText(2, formatMemory(p.vmRssKb));
            procItem->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
            procItem->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        }

        userItem->setExpanded(this->m_hasExpansionSnapshot
                              && this->m_expandedUsers.contains(uid));
    }

    this->ui->statusLabel->setText(tr("Logged in users: %1").arg(agg.size()));
}

quint64 UsersWidget::readTotalCpuJiffies()
{
    QFile f("/proc/stat");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return 0;

    const QByteArray line = f.readLine();
    f.close();

    const QList<QByteArray> parts = line.simplified().split(' ');
    quint64 total = 0;
    const int last = qMin(parts.size() - 1, 8);
    for (int i = 1; i <= last; ++i)
        total += parts.at(i).toULongLong();
    return total;
}

QString UsersWidget::formatMemory(quint64 kb)
{
    if (kb >= 1024ULL * 1024ULL)
        return QString::number(static_cast<double>(kb) / (1024.0 * 1024.0), 'f', 1) + " GB";
    if (kb >= 1024ULL)
        return QString::number(static_cast<double>(kb) / 1024.0, 'f', 1) + " MB";
    return QString::number(kb) + " KB";
}
