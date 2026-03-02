#include "processfilterproxy.h"
#include "logger.h"

#include <unistd.h>

namespace Os
{

ProcessFilterProxy::ProcessFilterProxy(QObject *parent) : QSortFilterProxyModel(parent)
{
    this->m_myUid = ::getuid();
    this->setSortRole(Qt::UserRole);
    this->setFilterRole(Qt::DisplayRole);
    this->setFilterCaseSensitivity(Qt::CaseInsensitive);
    this->setFilterKeyColumn(-1); // search across all columns
}

void ProcessFilterProxy::applyFilters()
{
    this->invalidateFilter();
}

bool ProcessFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const auto *model = qobject_cast<const ProcessModel *>(this->sourceModel());
    if (!model)
        return true;

    const QList<Process> &procs = model->processes();
    if (sourceRow < 0 || sourceRow >= procs.size())
        return false;

    const Process &proc = procs.at(sourceRow);

    // ── Kernel task filter ───────────────────────────────────────────────────
    if (!this->ShowKernelTasks && isKernelTask(proc))
        return false;

    // ── Other-users filter ───────────────────────────────────────────────────
    if (!this->ShowOtherUsersProcs && proc.uid != this->m_myUid)
    {
        LOG_INFO("Proc ID: " + QString::number(proc.uid));
        LOG_INFO("This ID: " + QString::number(this->m_myUid));
        return false;
    }

    // ── Free-text search (delegate to base class) ────────────────────────────
    return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}

// static
bool ProcessFilterProxy::isKernelTask(const Process &proc)
{
    // Kernel threads have an empty /proc/pid/cmdline; the isKernelThread flag
    // is set during Process::loadOne() before cmdline is filled with brackets.
    // Also unconditionally treat PID 1 and 2 as kernel roots.
    return proc.isKernelThread || proc.pid <= 2;
}

} // namespace Os
