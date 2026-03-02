#include "processmodel.h"
#include <QDateTime>
#include <unistd.h>

namespace Os
{

// ── Construction ──────────────────────────────────────────────────────────────

ProcessModel::ProcessModel(QObject *parent) : QAbstractTableModel(parent)
{
    this->m_numCpus = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    if (this->m_numCpus < 1)
        this->m_numCpus = 1;
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
            case ColPid:      return proc.pid;
            case ColName:     return proc.name;
            case ColUser:     return proc.user;
            case ColState:    return Process::stateString(proc.state);
            case ColCpu:      return QString::number(proc.cpuPercent, 'f', 1) + " %";
            case ColMemRss:   return formatMemory(proc.vmRssKb);
            case ColMemVirt:  return formatMemory(proc.vmSizeKb);
            case ColThreads:  return proc.threads;
            case ColPriority: return proc.priority;
            case ColNice:     return proc.nice;
            case ColCmdline:  return proc.cmdline;
            default: break;
        }
    }

    // Raw numeric values for sorting
    if (role == Qt::UserRole)
    {
        switch (static_cast<Column>(index.column()))
        {
            case ColPid:      return static_cast<qlonglong>(proc.pid);
            case ColCpu:      return proc.cpuPercent;
            case ColMemRss:   return static_cast<qulonglong>(proc.vmRssKb);
            case ColMemVirt:  return static_cast<qulonglong>(proc.vmSizeKb);
            case ColThreads:  return proc.threads;
            case ColPriority: return proc.priority;
            case ColNice:     return proc.nice;
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

void ProcessModel::refresh()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const double elapsedSec =
        (this->m_prevMs > 0) ? (nowMs - this->m_prevMs) / 1000.0 : 0.0;

    const long clkTck = sysconf(_SC_CLK_TCK);

    QList<Process> fresh = Process::loadAll();

    // Calculate CPU% per process using delta ticks
    for (Process &proc : fresh)
    {
        if (elapsedSec > 0.0 && this->m_prevTicks.contains(proc.pid))
        {
            const quint64 deltaTicks = proc.cpuTicks - this->m_prevTicks.value(proc.pid);
            proc.cpuPercent =
                (static_cast<double>(deltaTicks)
                 / (elapsedSec * this->m_numCpus * clkTck))
                * 100.0;
        }
    }

    // Store tick snapshot for next sample
    this->m_prevTicks.clear();
    for (const Process &proc : fresh)
        this->m_prevTicks.insert(proc.pid, proc.cpuTicks);
    this->m_prevMs = nowMs;

    beginResetModel();
    this->m_processes = std::move(fresh);
    endResetModel();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QString ProcessModel::formatMemory(quint64 kb)
{
    if (kb >= 1024ULL * 1024ULL)
        return QString::number(kb / (1024ULL * 1024ULL)) + " GB";
    if (kb >= 1024ULL)
        return QString::number(kb / 1024ULL) + " MB";
    return QString::number(kb) + " KB";
}

QString ProcessModel::columnHeader(Column col)
{
    switch (col)
    {
        case ColPid:      return "PID";
        case ColName:     return "Name";
        case ColUser:     return "User";
        case ColState:    return "State";
        case ColCpu:      return "CPU %";
        case ColMemRss:   return "Memory";
        case ColMemVirt:  return "Virtual";
        case ColThreads:  return "Threads";
        case ColPriority: return "Priority";
        case ColNice:     return "Nice";
        case ColCmdline:  return "Command";
        default:          return {};
    }
}

} // namespace Os
