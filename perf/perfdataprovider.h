#ifndef PERF_PERFDATAPROVIDER_H
#define PERF_PERFDATAPROVIDER_H

#include <QObject>
#include <QTimer>
#include <QVector>

namespace Perf
{

/// Number of historical samples kept per metric.
static constexpr int HISTORY_SIZE = 60;

/// Periodically samples /proc/stat (CPU) and /proc/meminfo (memory).
/// All widgets that display performance data connect to the updated() signal
/// and read values through the const accessors.
class PerfDataProvider : public QObject
{
    Q_OBJECT

    public:
        explicit PerfDataProvider(QObject *parent = nullptr);

        void setInterval(int ms);

        // ── CPU ──────────────────────────────────────────────────────────────
        double cpuPercent()    const { return this->m_cpuHistory.isEmpty() ? 0.0
                                              : this->m_cpuHistory.last(); }
        const QVector<double> &cpuHistory()  const { return this->m_cpuHistory;  }

        // ── Memory ───────────────────────────────────────────────────────────
        qint64 memTotalKb()   const { return this->m_memTotalKb;   }
        qint64 memUsedKb()    const { return this->m_memUsedKb;    }
        qint64 memAvailKb()   const { return this->m_memAvailKb;   }
        qint64 memCachedKb()  const { return this->m_memCachedKb;  }
        qint64 memBuffersKb() const { return this->m_memBuffersKb; }

        /// Memory used as a fraction 0..1
        double memFraction()  const;

        const QVector<double> &memHistory()  const { return this->m_memHistory;  }

    signals:
        void updated();

    private slots:
        void onTimer();

    private:
        QTimer *m_timer;

        // CPU state
        quint64          m_prevCpuIdle  { 0 };
        quint64          m_prevCpuTotal { 0 };
        QVector<double>  m_cpuHistory;

        // Memory state
        qint64           m_memTotalKb   { 0 };
        qint64           m_memUsedKb    { 0 };
        qint64           m_memAvailKb   { 0 };
        qint64           m_memCachedKb  { 0 };
        qint64           m_memBuffersKb { 0 };
        QVector<double>  m_memHistory;

        bool sampleCpu();
        bool sampleMemory();

        static void appendHistory(QVector<double> &vec, double val);
};

} // namespace Perf

#endif // PERF_PERFDATAPROVIDER_H
