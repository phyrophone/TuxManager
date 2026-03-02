#ifndef PERF_PERFDATAPROVIDER_H
#define PERF_PERFDATAPROVIDER_H

#include <QObject>
#include <QTimer>
#include <QVector>
#include <QString>
#include <QElapsedTimer>
#include <QSet>
#include <QHash>

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
        void setActive(bool active);
        bool isActive() const { return this->m_active; }
        void setProcessStatsEnabled(bool enabled) { this->m_processStatsEnabled = enabled; }

        // ── Aggregate CPU ─────────────────────────────────────────────────────
        double cpuPercent()  const { return this->m_cpuHistory.isEmpty() ? 0.0
                                            : this->m_cpuHistory.last(); }
        const QVector<double> &cpuHistory()       const { return this->m_cpuHistory; }
        const QVector<double> &cpuKernelHistory() const { return this->m_cpuKernelHistory; }

        // ── Per-core CPU ──────────────────────────────────────────────────────
        int    coreCount()                          const { return this->m_cores.size(); }
        double corePercent(int i)                   const;
        const  QVector<double> &coreHistory(int i)       const;
        const  QVector<double> &coreKernelHistory(int i) const;

        // ── CPU metadata (read once at startup) ───────────────────────────────
        const QString &cpuModelName() const { return this->m_cpuModelName; }
        double cpuBaseMhz()           const { return this->m_cpuBaseMhz;   }
        double cpuCurrentMhz()        const { return this->m_cpuCurrentMhz; }
        int    cpuLogicalCount()      const { return this->m_cpuLogicalCount; }
        bool   cpuIsVirtualMachine()  const { return this->m_cpuIsVirtualMachine; }
        const QString &cpuVmVendor()  const { return this->m_cpuVmVendor; }

        // ── Process / thread counts (updated every sample) ────────────────────
        int processCount() const { return this->m_processCount; }
        int threadCount()  const { return this->m_threadCount;  }

        // ── Memory ───────────────────────────────────────────────────────────
        qint64 memTotalKb()   const { return this->m_memTotalKb;   }
        /// In-use (htop formula): Total - Free - Buffers - PageCache
        qint64 memUsedKb()    const { return this->m_memUsedKb;    }
        qint64 memAvailKb()   const { return this->m_memAvailKb;   }
        /// Truly free (MemFree from /proc/meminfo)
        qint64 memFreeKb()    const { return this->m_memFreeKb;    }
        /// Page cache: Cached + SReclaimable - Shmem + Buffers
        qint64 memCachedKb()  const { return this->m_memCachedKb;  }
        qint64 memBuffersKb() const { return this->m_memBuffersKb; }
        /// Dirty pages pending write-back: Dirty + Writeback
        qint64 memDirtyKb()   const { return this->m_memDirtyKb;   }
        int memDimmSlotsTotal() const { return this->m_memDimmSlotsTotal; }
        int memDimmSlotsUsed()  const { return this->m_memDimmSlotsUsed;  }
        int memSpeedMtps()      const { return this->m_memSpeedMtps;      }
        double memFraction()  const;
        const QVector<double> &memHistory() const { return this->m_memHistory; }

        // ── Disks (base block devices backing mounted/swap paths) ────────────
        int diskCount() const { return this->m_disks.size(); }
        QString diskName(int i) const;
        QString diskModel(int i) const;
        QString diskType(int i) const;
        double diskActivePercent(int i) const;
        double diskReadBytesPerSec(int i) const;
        double diskWriteBytesPerSec(int i) const;
        qint64 diskCapacityBytes(int i) const;
        qint64 diskFormattedBytes(int i) const;
        bool diskIsSystemDisk(int i) const;
        bool diskHasPageFile(int i) const;
        const QVector<double> &diskActiveHistory(int i) const;
        const QVector<double> &diskReadHistory(int i) const;
        const QVector<double> &diskWriteHistory(int i) const;

        // ── GPUs (tooling-backed, currently nvidia-smi) ──────────────────────
        int gpuCount() const { return this->m_gpus.size(); }
        QString gpuName(int i) const;
        QString gpuDriverVersion(int i) const;
        QString gpuBackendName(int i) const;
        double gpuUtilPercent(int i) const;
        qint64 gpuMemUsedMiB(int i) const;
        qint64 gpuMemTotalMiB(int i) const;
        const QVector<double> &gpuUtilHistory(int i) const;
        const QVector<double> &gpuMemUsageHistory(int i) const;
        int gpuEngineCount(int gpuIndex) const;
        QString gpuEngineName(int gpuIndex, int engineIndex) const;
        double gpuEnginePercent(int gpuIndex, int engineIndex) const;
        const QVector<double> &gpuEngineHistory(int gpuIndex, int engineIndex) const;

    signals:
        void updated();

    private slots:
        void onTimer();

    private:
        /// Per-core rolling sample state.
        struct CoreSample
        {
            quint64        prevIdle   { 0 };
            quint64        prevTotal  { 0 };
            quint64        prevKernel { 0 };
            QVector<double> history;
            QVector<double> kernelHistory;
        };

        struct DiskSample
        {
            QString        name;          ///< base device name (e.g. sda, nvme0n1)
            QString        model;         ///< best-effort model string
            QString        type;          ///< HDD/SSD/Unknown
            quint64        prevReadSecs  { 0 };
            quint64        prevWriteSecs { 0 };
            quint64        prevIoMs      { 0 };
            double         activePct     { 0.0 };
            double         readBps       { 0.0 };
            double         writeBps      { 0.0 };
            qint64         capacityBytes { 0 };
            qint64         formattedBytes { 0 };
            bool           isSystemDisk { false };
            bool           hasPageFile { false };
            QVector<double> activeHistory;
            QVector<double> readHistory;
            QVector<double> writeHistory;
        };

        struct GpuEngineSample
        {
            QString         key;
            QString         label;
            double          pct { 0.0 };
            QVector<double> history;
        };

        struct GpuSample
        {
            QString         id;
            QString         name;
            QString         driverVersion;
            QString         backend;
            double          utilPct { 0.0 };
            qint64          memUsedMiB { 0 };
            qint64          memTotalMiB { 0 };
            QVector<double> utilHistory;
            QVector<double> memUsageHistory;
            QVector<GpuEngineSample> engines;
        };

        QTimer *m_timer;
        int     m_intervalMs { 1000 };
        bool    m_active { true };
        bool    m_processStatsEnabled { false };

        // Aggregate CPU state
        quint64          m_prevCpuIdle   { 0 };
        quint64          m_prevCpuTotal  { 0 };
        quint64          m_prevCpuKernel { 0 };
        QVector<double>  m_cpuHistory;
        QVector<double>  m_cpuKernelHistory;

        // Per-core state
        QVector<CoreSample> m_cores;

        // CPU metadata
        QString  m_cpuModelName;
        double   m_cpuBaseMhz       { 0.0 };
        double   m_cpuCurrentMhz    { 0.0 };
        int      m_cpuLogicalCount  { 0 };
        bool     m_cpuIsVirtualMachine { false };
        QString  m_cpuVmVendor;

        // Process/thread counts
        int      m_processCount { 0 };
        int      m_threadCount  { 0 };

        // Memory state
        qint64           m_memTotalKb   { 0 };
        qint64           m_memUsedKb    { 0 };
        qint64           m_memAvailKb   { 0 };
        qint64           m_memFreeKb    { 0 };
        qint64           m_memCachedKb  { 0 };
        qint64           m_memBuffersKb { 0 };
        qint64           m_memDirtyKb   { 0 };
        int              m_memDimmSlotsTotal { 0 };
        int              m_memDimmSlotsUsed  { 0 };
        int              m_memSpeedMtps      { 0 };
        QVector<double>  m_memHistory;

        // Disk state
        QVector<DiskSample> m_disks;
        QElapsedTimer       m_diskTimer;
        qint64              m_prevDiskSampleMs { 0 };

        // GPU state
        QVector<GpuSample>  m_gpus;
        bool                m_hasNvidiaSmi { false };
        QString             m_nvidiaSmiPath;

        bool sampleCpu();
        bool sampleMemory();
        bool sampleDisks();
        bool sampleGpus();
        void sampleProcessStats();
        void readCpuMetadata();
        void readCurrentFreq();
        void readHardwareMetadata();
        void refreshDisks(const QSet<QString> &measurableDevices);
        void detectGpuBackends();
        bool sampleNvidiaSmi();
        static double parsePercentField(const QString &field);
        static qint64 parseMiBField(const QString &field);

        static QSet<QString> resolveBaseBlockDevices(const QString &devName);
        static bool shouldIgnoreBlockDevice(const QString &baseName);
        static QString readSysTextFile(const QString &path);
        static quint16 readLe16(const QByteArray &raw, int off);
        static quint32 readLe32(const QByteArray &raw, int off);

        static void   appendHistory(QVector<double> &vec, double val);
        static quint64 parseCpuLine(const QList<QByteArray> &parts,
                                    quint64 &outIdle, quint64 &outKernel);
};

} // namespace Perf

#endif // PERF_PERFDATAPROVIDER_H
