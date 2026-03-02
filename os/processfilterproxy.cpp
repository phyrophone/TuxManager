#include "processfilterproxy.h"

#include <unistd.h>

namespace Os
{

ProcessFilterProxy::ProcessFilterProxy(QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_myUid(::getuid())
{
    this->setSortRole(Qt::UserRole);
    this->setFilterRole(Qt::DisplayRole);
    this->setFilterCaseSensitivity(Qt::CaseInsensitive);
    this->setFilterKeyColumn(-1); // search across all columns
}

void ProcessFilterProxy::applyFilters()
{
    this->invalidateFilter();
}

bool ProcessFilterProxy::filterAcceptsRow(int sourceRow,
                                          const QModelIndex &sourceParent) const
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
        return false;

    // ── Free-text search (delegate to base class) ────────────────────────────
    return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}

// static
bool ProcessFilterProxy::isKernelTask(const Process &proc)
{
    // Rely solely on the PF_KTHREAD flag read from /proc/pid/stat (same as
    // htop). PID 1 (systemd/init) is NOT a kernel thread and must NOT be
    // hidden. PID 2 (kthreadd) has PF_KTHREAD set so it is covered naturally.
    return proc.isKernelThread;
}

} // namespace Os
