#ifndef OS_PROCESSFILTERPROXY_H
#define OS_PROCESSFILTERPROXY_H

#include "processmodel.h"

#include <QSortFilterProxyModel>
#include <sys/types.h>

namespace Os
{

/// Extended sort/filter proxy for the process table.
///
/// Combines free-text search (inherited from QSortFilterProxyModel) with
/// two additional visibility toggles:
///   - ShowKernelTasks      – show/hide kernel threads
///   - ShowOtherUsersProcs  – show/hide processes owned by other UIDs
///
/// Kernel tasks are identified by a name enclosed in brackets, e.g. [kworker].
class ProcessFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT

    public:
        explicit ProcessFilterProxy(QObject *parent = nullptr);

        bool ShowKernelTasks     { true };
        bool ShowOtherUsersProcs { true };

        /// Call after toggling either flag to re-run the filter.
        void applyFilters();

    protected:
        bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

    private:
        uid_t m_myUid;
        static bool isKernelTask(const Process &proc);
};

} // namespace Os

#endif // OS_PROCESSFILTERPROXY_H
