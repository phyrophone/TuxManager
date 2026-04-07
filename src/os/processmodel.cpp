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

#include "processmodel.h"
#include "../misc.h"
#include <QFile>
#include <unistd.h>

using namespace OS;

// ── Construction ──────────────────────────────────────────────────────────────

ProcessModel::ProcessModel(QObject *parent) : QAbstractTableModel(parent)
{
    this->m_numCpus = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    if (this->m_numCpus < 1)
        this->m_numCpus = 1;
    this->m_myUid = ::getuid();
}

// ── QAbstractTableModel interface ─────────────────────────────────────────────

int ProcessModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return this->m_processes.size();
}

int ProcessModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return ColCount;
}

QVariant ProcessModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()
        || index.row()    < 0
        || index.row()    >= this->m_processes.size()
        || index.column() < 0
        || index.column() >= ColCount)
        return {};

    const Process &proc = this->m_processes.at(index.row());

    if (role == Qt::DisplayRole)
    {
        switch (static_cast<Column>(index.column()))
        {
            case ColPid:      return proc.PID;
            case ColName:     return proc.Name;
            case ColUser:     return proc.User;
            case ColState:    return Process::GetStateString(proc.State);
            case ColCpu:      return QString::number(proc.CPUPercent, 'f', 1) + " %";
            case ColMemRss:   return Misc::FormatKiB(proc.VMRssKb, 0);
            case ColMemVirt:  return Misc::FormatKiB(proc.vmSizeKb, 0);
            case ColThreads:  return proc.Threads;
            case ColPriority: return proc.Priority;
            case ColNice:     return proc.Nice;
            case ColCmdline:  return proc.CmdLine;
            default: break;
        }
    }

    // Raw numeric values for sorting
    if (role == Qt::UserRole)
    {
        switch (static_cast<Column>(index.column()))
        {
            case ColPid:      return static_cast<qlonglong>(proc.PID);
            case ColCpu:      return proc.CPUPercent;
            case ColMemRss:   return static_cast<qulonglong>(proc.VMRssKb);
            case ColMemVirt:  return static_cast<qulonglong>(proc.vmSizeKb);
            case ColThreads:  return proc.Threads;
            case ColPriority: return proc.Priority;
            case ColNice:     return proc.Nice;
            default:          return this->data(index, Qt::DisplayRole);
        }
    }

    if (role == Qt::TextAlignmentRole)
    {
        switch (static_cast<Column>(index.column()))
        {
            case ColPid:
            case ColCpu:
            case ColMemRss:
            case ColMemVirt:
            case ColThreads:
            case ColPriority:
            case ColNice:
                return QVariant(Qt::AlignRight | Qt::AlignVCenter);
            default:
                return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    return {};
}

QVariant ProcessModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    if (section < 0 || section >= ColCount)
        return {};
    return columnHeader(static_cast<Column>(section));
}

Qt::ItemFlags ProcessModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

// ── Refresh ───────────────────────────────────────────────────────────────────

void ProcessModel::Refresh()
{
    // Read total elapsed CPU jiffies (all CPUs, all states) from /proc/stat.
    // Using the actual CPU time budget as the denominator — rather than wall
    // clock time — matches htop's approach and gives accurate results even
    // when the timer fires slightly early or late.
    const quint64 totalJiffies = readTotalCpuJiffies();
    const quint64 periodJiffies =
        (this->m_prevCpuTotalTicks > 0 && totalJiffies > this->m_prevCpuTotalTicks)
        ? (totalJiffies - this->m_prevCpuTotalTicks) : 0;

    Process::LoadOptions opts;
    opts.IncludeKernelTasks = this->m_showKernelTasks;
    opts.IncludeOtherUsers  = this->m_showOtherUsersProcs;
    opts.MyUID              = this->m_myUid;
    QList<Process> fresh = Process::LoadAll(opts);

    // Calculate CPU% per process: (delta process ticks) / (period per CPU) * 100
    if (periodJiffies > 0)
    {
        const double periodPerCpu =
            static_cast<double>(periodJiffies) / this->m_numCpus;

        for (Process &proc : fresh)
        {
            if (this->m_prevTicks.contains(proc.PID))
            {
                const quint64 prevTicks = this->m_prevTicks.value(proc.PID);
                if (proc.CPUTicks >= prevTicks)
                {
                    const double pct = static_cast<double>(proc.CPUTicks - prevTicks) / periodPerCpu * 100.0;
                    // Cap at 100 % × num_cpus (matches htop's MINIMUM() clamp)
                    proc.CPUPercent = qMin(pct, 100.0 * this->m_numCpus);
                }
            }
        }
    }

    // Store snapshots for next sample
    this->m_prevTicks.clear();
    for (const Process &proc : fresh)
        this->m_prevTicks.insert(proc.PID, proc.CPUTicks);
    this->m_prevCpuTotalTicks = totalJiffies;

    this->beginResetModel();
    this->m_processes = std::move(fresh);
    this->endResetModel();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

quint64 ProcessModel::readTotalCpuJiffies()
{
    QFile f("/proc/stat");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return 0;

    const QByteArray line = f.readLine(); // first line: "cpu  user nice system ..."
    f.close();

    // Format: cpu user nice system idle iowait irq softirq steal [guest guestnice]
    // guest/guestnice are already included in user/nice, so sum only fields 1–8.
    const QList<QByteArray> parts = line.simplified().split(' ');
    quint64 total = 0;
    // parts[0] = "cpu", parts[1..8] = user nice system idle iowait irq softirq steal
    const int last = qMin(parts.size() - 1, 8);
    for (int i = 1; i <= last; ++i)
        total += parts[i].toULongLong();
    return total;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QString ProcessModel::columnHeader(Column col)
{
    switch (col)
    {
        case ColPid:      return "PID";
        case ColName:     return "Name";
        case ColUser:     return "User";
        case ColState:    return "State";
        case ColCpu:      return "CPU %";
        case ColMemRss:   return "MEM RES";
        case ColMemVirt:  return "MEM VIRT";
        case ColThreads:  return "Threads";
        case ColPriority: return "Priority";
        case ColNice:     return "Nice";
        case ColCmdline:  return "Command";
        default:          return {};
    }
}
