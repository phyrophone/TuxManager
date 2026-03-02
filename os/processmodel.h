#ifndef OS_PROCESSMODEL_H
#define OS_PROCESSMODEL_H

#include "process.h"

#include <QAbstractTableModel>
#include <QHash>

namespace Os
{

class ProcessModel : public QAbstractTableModel
{
    Q_OBJECT

    public:
        /// Logical columns — order in the enum defines default display order.
        enum Column
        {
            ColPid = 0,
            ColName,
            ColUser,
            ColState,
            ColCpu,
            ColMemRss,
            ColMemVirt,
            ColThreads,
            ColPriority,
            ColNice,
            ColCmdline,
            ColCount           ///< Sentinel — always last
        };

        explicit ProcessModel(QObject *parent = nullptr);

        // QAbstractTableModel interface
        int      rowCount(const QModelIndex &parent = QModelIndex()) const override;
        int      columnCount(const QModelIndex &parent = QModelIndex()) const override;
        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        QVariant headerData(int section, Qt::Orientation orientation,
                            int role = Qt::DisplayRole) const override;
        Qt::ItemFlags flags(const QModelIndex &index) const override;

        /// Reload all process data from /proc and update CPU%.
        /// Call this on a timer tick.
        void refresh();

        /// The raw process list (read-only access for external use).
        const QList<Process> &processes() const { return this->m_processes; }

    private:
        QList<Process>          m_processes;
        QHash<pid_t, quint64>   m_prevTicks;      ///< cpuTicks from previous sample
        qint64                  m_prevMs { 0 };   ///< Timestamp of previous sample (ms)
        int                     m_numCpus { 1 };  ///< Online CPU count for normalisation

        static QString formatMemory(quint64 kb);
        static QString columnHeader(Column col);
};

} // namespace Os

#endif // OS_PROCESSMODEL_H
