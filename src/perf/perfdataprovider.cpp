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

#include "perfdataprovider.h"

#include "logger.h"

#include <algorithm>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QStringList>
#include <dlfcn.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <unistd.h>

using namespace Perf;

namespace
{
using NvmlReturn = unsigned int;
using NvmlDevice = void *;

struct NvmlUtilization
{
    unsigned int gpu;
    unsigned int memory;
};

struct NvmlMemory
{
    uint64_t total;
    uint64_t free;
    uint64_t used;
};

static constexpr NvmlReturn NVML_SUCCESS = 0;

using FnNvmlInitV2 = NvmlReturn (*)();
using FnNvmlShutdown = NvmlReturn (*)();
using FnNvmlSystemGetDriverVersion = NvmlReturn (*)(char *, unsigned int);
using FnNvmlDeviceGetCountV2 = NvmlReturn (*)(unsigned int *);
using FnNvmlDeviceGetHandleByIndexV2 = NvmlReturn (*)(unsigned int, NvmlDevice *);
using FnNvmlDeviceGetName = NvmlReturn (*)(NvmlDevice, char *, unsigned int);
using FnNvmlDeviceGetUUID = NvmlReturn (*)(NvmlDevice, char *, unsigned int);
using FnNvmlDeviceGetUtilizationRates = NvmlReturn (*)(NvmlDevice, NvmlUtilization *);
using FnNvmlDeviceGetMemoryInfo = NvmlReturn (*)(NvmlDevice, NvmlMemory *);
using FnNvmlDeviceGetEncoderUtilization = NvmlReturn (*)(NvmlDevice, unsigned int *, unsigned int *);
using FnNvmlDeviceGetDecoderUtilization = NvmlReturn (*)(NvmlDevice, unsigned int *, unsigned int *);
using FnNvmlDeviceGetPcieThroughput = NvmlReturn (*)(NvmlDevice, unsigned int, unsigned int *);
using FnNvmlDeviceGetTemperature = NvmlReturn (*)(NvmlDevice, unsigned int, unsigned int *);

FnNvmlInitV2 pNvmlInitV2 = nullptr;
FnNvmlShutdown pNvmlShutdown = nullptr;
FnNvmlSystemGetDriverVersion pNvmlSystemGetDriverVersion = nullptr;
FnNvmlDeviceGetCountV2 pNvmlDeviceGetCountV2 = nullptr;
FnNvmlDeviceGetHandleByIndexV2 pNvmlDeviceGetHandleByIndexV2 = nullptr;
FnNvmlDeviceGetName pNvmlDeviceGetName = nullptr;
FnNvmlDeviceGetUUID pNvmlDeviceGetUUID = nullptr;
FnNvmlDeviceGetUtilizationRates pNvmlDeviceGetUtilizationRates = nullptr;
FnNvmlDeviceGetMemoryInfo pNvmlDeviceGetMemoryInfo = nullptr;
FnNvmlDeviceGetEncoderUtilization pNvmlDeviceGetEncoderUtilization = nullptr;
FnNvmlDeviceGetDecoderUtilization pNvmlDeviceGetDecoderUtilization = nullptr;
FnNvmlDeviceGetPcieThroughput pNvmlDeviceGetPcieThroughput = nullptr;
FnNvmlDeviceGetTemperature pNvmlDeviceGetTemperature = nullptr;

bool textContainsAnyToken(const QString &text, const QStringList &tokens)
{
    const QString lower = text.toLower();
    for (const QString &t : tokens)
    {
        if (lower.contains(t))
            return true;
    }
    return false;
}
}

// ── Construction ──────────────────────────────────────────────────────────────

PerfDataProvider::PerfDataProvider(QObject *parent) : QObject(parent), m_timer(new QTimer(this))
{
    connect(this->m_timer, &QTimer::timeout, this, &PerfDataProvider::onTimer);

    this->readCpuMetadata();
    this->readHardwareMetadata();
    this->detectCpuTemperatureSensor();
    this->detectGpuBackends();

    // Prime baselines — first real sample will have valid deltas.
    if (this->m_cpuSamplingEnabled)
        this->sampleCpu();
    this->sampleCpuTemperature();
    if (this->m_memorySamplingEnabled)
        this->sampleMemory();
    if (this->m_diskSamplingEnabled)
        this->sampleDisks();
    if (this->m_networkSamplingEnabled)
        this->sampleNetworks();
    if (this->m_gpuSamplingEnabled)
        this->sampleGpus();
    if (this->m_processStatsEnabled)
        this->sampleProcessStats();

    this->m_timer->start(1000);
}

PerfDataProvider::~PerfDataProvider()
{
    this->unloadGpuBackends();
}

void PerfDataProvider::SetInterval(int ms)
{
    this->m_intervalMs = qMax(100, ms);
    this->m_timer->setInterval(this->m_intervalMs);
}

void PerfDataProvider::SetActive(bool active)
{
    if (this->m_active == active)
        return;

    this->m_active = active;
    if (active)
    {
        // Refresh once immediately when entering Performance tab.
        if (this->m_cpuSamplingEnabled)
            this->sampleCpu();
        this->sampleCpuTemperature();
        if (this->m_memorySamplingEnabled)
            this->sampleMemory();
        if (this->m_diskSamplingEnabled)
            this->sampleDisks();
        if (this->m_networkSamplingEnabled)
            this->sampleNetworks();
        if (this->m_gpuSamplingEnabled)
            this->sampleGpus();
        if (this->m_processStatsEnabled)
            this->sampleProcessStats();
        if (this->m_cpuSamplingEnabled)
            this->readCurrentFreq();
        emit this->updated();
        this->m_timer->start(this->m_intervalMs);
    }
    else
    {
        this->m_timer->stop();
    }
}

double PerfDataProvider::MemFraction() const
{
    if (this->m_memTotalKb <= 0)
        return 0.0;
    return static_cast<double>(this->m_memUsedKb)
           / static_cast<double>(this->m_memTotalKb);
}

double PerfDataProvider::CorePercent(int i) const
{
    if (i < 0 || i >= this->m_cores.size())
        return 0.0;
    const auto &c = this->m_cores.at(i);
    return c.history.isEmpty() ? 0.0 : c.history.last();
}

const QVector<double> &PerfDataProvider::CoreHistory(int i) const
{
    static const QVector<double> empty;
    if (i < 0 || i >= this->m_cores.size())
        return empty;
    return this->m_cores.at(i).history;
}

const QVector<double> &PerfDataProvider::CoreKernelHistory(int i) const
{
    static const QVector<double> empty;
    if (i < 0 || i >= this->m_cores.size())
        return empty;
    return this->m_cores.at(i).kernelHistory;
}

double PerfDataProvider::CoreCurrentMhz(int i) const
{
    if (i < 0 || i >= this->m_coreCurrentMhz.size())
        return 0.0;
    return this->m_coreCurrentMhz.at(i);
}

QString PerfDataProvider::DiskName(int i) const
{
    if (i < 0 || i >= this->m_disks.size())
        return {};
    return this->m_disks.at(i).name;
}

QString PerfDataProvider::DiskModel(int i) const
{
    if (i < 0 || i >= this->m_disks.size())
        return {};
    return this->m_disks.at(i).model;
}

QString PerfDataProvider::DiskType(int i) const
{
    if (i < 0 || i >= this->m_disks.size())
        return {};
    return this->m_disks.at(i).type;
}

double PerfDataProvider::DiskActivePercent(int i) const
{
    if (i < 0 || i >= this->m_disks.size())
        return 0.0;
    return this->m_disks.at(i).activePct;
}

double PerfDataProvider::DiskReadBytesPerSec(int i) const
{
    if (i < 0 || i >= this->m_disks.size())
        return 0.0;
    return this->m_disks.at(i).readBps;
}

double PerfDataProvider::DiskWriteBytesPerSec(int i) const
{
    if (i < 0 || i >= this->m_disks.size())
        return 0.0;
    return this->m_disks.at(i).writeBps;
}

qint64 PerfDataProvider::DiskCapacityBytes(int i) const
{
    if (i < 0 || i >= this->m_disks.size())
        return 0;
    return this->m_disks.at(i).capacityBytes;
}

qint64 PerfDataProvider::DiskFormattedBytes(int i) const
{
    if (i < 0 || i >= this->m_disks.size())
        return 0;
    return this->m_disks.at(i).formattedBytes;
}

bool PerfDataProvider::DiskIsSystemDisk(int i) const
{
    if (i < 0 || i >= this->m_disks.size())
        return false;
    return this->m_disks.at(i).isSystemDisk;
}

bool PerfDataProvider::DiskHasPageFile(int i) const
{
    if (i < 0 || i >= this->m_disks.size())
        return false;
    return this->m_disks.at(i).hasPageFile;
}

const QVector<double> &PerfDataProvider::DiskActiveHistory(int i) const
{
    static const QVector<double> empty;
    if (i < 0 || i >= this->m_disks.size())
        return empty;
    return this->m_disks.at(i).activeHistory;
}

const QVector<double> &PerfDataProvider::DiskReadHistory(int i) const
{
    static const QVector<double> empty;
    if (i < 0 || i >= this->m_disks.size())
        return empty;
    return this->m_disks.at(i).readHistory;
}

const QVector<double> &PerfDataProvider::DiskWriteHistory(int i) const
{
    static const QVector<double> empty;
    if (i < 0 || i >= this->m_disks.size())
        return empty;
    return this->m_disks.at(i).writeHistory;
}

QString PerfDataProvider::NetworkName(int i) const
{
    if (i < 0 || i >= this->m_networks.size())
        return {};
    return this->m_networks.at(i).name;
}

QString PerfDataProvider::NetworkType(int i) const
{
    if (i < 0 || i >= this->m_networks.size())
        return {};
    return this->m_networks.at(i).type;
}

int PerfDataProvider::NetworkLinkSpeedMbps(int i) const
{
    if (i < 0 || i >= this->m_networks.size())
        return 0;
    return this->m_networks.at(i).linkSpeedMbps;
}

QString PerfDataProvider::NetworkIpv4(int i) const
{
    if (i < 0 || i >= this->m_networks.size())
        return {};
    return this->m_networks.at(i).ipv4;
}

QString PerfDataProvider::NetworkIpv6(int i) const
{
    if (i < 0 || i >= this->m_networks.size())
        return {};
    return this->m_networks.at(i).ipv6;
}

double PerfDataProvider::NetworkRxBytesPerSec(int i) const
{
    if (i < 0 || i >= this->m_networks.size())
        return 0.0;
    return this->m_networks.at(i).rxBps;
}

double PerfDataProvider::NetworkTxBytesPerSec(int i) const
{
    if (i < 0 || i >= this->m_networks.size())
        return 0.0;
    return this->m_networks.at(i).txBps;
}

const QVector<double> &PerfDataProvider::NetworkRxHistory(int i) const
{
    static const QVector<double> empty;
    if (i < 0 || i >= this->m_networks.size())
        return empty;
    return this->m_networks.at(i).rxHistory;
}

const QVector<double> &PerfDataProvider::NetworkTxHistory(int i) const
{
    static const QVector<double> empty;
    if (i < 0 || i >= this->m_networks.size())
        return empty;
    return this->m_networks.at(i).txHistory;
}

QString PerfDataProvider::GpuName(int i) const
{
    if (i < 0 || i >= this->m_gpus.size())
        return {};
    return this->m_gpus.at(i).name;
}

QString PerfDataProvider::GpuDriverVersion(int i) const
{
    if (i < 0 || i >= this->m_gpus.size())
        return {};
    return this->m_gpus.at(i).driverVersion;
}

QString PerfDataProvider::GpuBackendName(int i) const
{
    if (i < 0 || i >= this->m_gpus.size())
        return {};
    return this->m_gpus.at(i).backend;
}

double PerfDataProvider::GpuUtilPercent(int i) const
{
    if (i < 0 || i >= this->m_gpus.size())
        return 0.0;
    return this->m_gpus.at(i).utilPct;
}

int PerfDataProvider::GpuTemperatureC(int i) const
{
    if (i < 0 || i >= this->m_gpus.size())
        return -1;
    return this->m_gpus.at(i).temperatureC;
}

qint64 PerfDataProvider::GpuMemUsedMiB(int i) const
{
    if (i < 0 || i >= this->m_gpus.size())
        return 0;
    return this->m_gpus.at(i).memUsedMiB;
}

qint64 PerfDataProvider::GpuMemTotalMiB(int i) const
{
    if (i < 0 || i >= this->m_gpus.size())
        return 0;
    return this->m_gpus.at(i).memTotalMiB;
}

const QVector<double> &PerfDataProvider::GpuUtilHistory(int i) const
{
    static const QVector<double> empty;
    if (i < 0 || i >= this->m_gpus.size())
        return empty;
    return this->m_gpus.at(i).utilHistory;
}

const QVector<double> &PerfDataProvider::GpuMemUsageHistory(int i) const
{
    static const QVector<double> empty;
    if (i < 0 || i >= this->m_gpus.size())
        return empty;
    return this->m_gpus.at(i).memUsageHistory;
}

const QVector<double> &PerfDataProvider::GpuCopyTxHistory(int i) const
{
    static const QVector<double> empty;
    if (i < 0 || i >= this->m_gpus.size())
        return empty;
    return this->m_gpus.at(i).copyTxHistory;
}

const QVector<double> &PerfDataProvider::GpuCopyRxHistory(int i) const
{
    static const QVector<double> empty;
    if (i < 0 || i >= this->m_gpus.size())
        return empty;
    return this->m_gpus.at(i).copyRxHistory;
}

int PerfDataProvider::GpuEngineCount(int gpuIndex) const
{
    if (gpuIndex < 0 || gpuIndex >= this->m_gpus.size())
        return 0;
    return this->m_gpus.at(gpuIndex).engines.size();
}

QString PerfDataProvider::GpuEngineName(int gpuIndex, int engineIndex) const
{
    if (gpuIndex < 0 || gpuIndex >= this->m_gpus.size())
        return {};
    const auto &engines = this->m_gpus.at(gpuIndex).engines;
    if (engineIndex < 0 || engineIndex >= engines.size())
        return {};
    return engines.at(engineIndex).label;
}

double PerfDataProvider::GpuEnginePercent(int gpuIndex, int engineIndex) const
{
    if (gpuIndex < 0 || gpuIndex >= this->m_gpus.size())
        return 0.0;
    const auto &engines = this->m_gpus.at(gpuIndex).engines;
    if (engineIndex < 0 || engineIndex >= engines.size())
        return 0.0;
    return engines.at(engineIndex).pct;
}

const QVector<double> &PerfDataProvider::GpuEngineHistory(int gpuIndex, int engineIndex) const
{
    static const QVector<double> empty;
    if (gpuIndex < 0 || gpuIndex >= this->m_gpus.size())
        return empty;
    const auto &engines = this->m_gpus.at(gpuIndex).engines;
    if (engineIndex < 0 || engineIndex >= engines.size())
        return empty;
    return engines.at(engineIndex).history;
}

// ── Private slots ─────────────────────────────────────────────────────────────

void PerfDataProvider::onTimer()
{
    if (!this->m_active)
        return;

    if (this->m_cpuSamplingEnabled)
        this->sampleCpu();
    this->sampleCpuTemperature();
    if (this->m_memorySamplingEnabled)
        this->sampleMemory();
    if (this->m_diskSamplingEnabled)
        this->sampleDisks();
    if (this->m_networkSamplingEnabled)
        this->sampleNetworks();
    if (this->m_gpuSamplingEnabled)
        this->sampleGpus();
    if (this->m_processStatsEnabled)
        this->sampleProcessStats();
    if (this->m_cpuSamplingEnabled)
        this->readCurrentFreq();
    emit this->updated();
}

// ── CPU sampling ──────────────────────────────────────────────────────────────

// Parse one "cpu..." line from /proc/stat — returns total jiffies and writes
// idle and kernel jiffies via output parameters.
quint64 PerfDataProvider::parseCpuLine(const QList<QByteArray> &parts,
                                       quint64 &outIdle,
                                       quint64 &outKernel)
{
    // Fields (1-indexed after the label):
    // 1:user 2:nice 3:system 4:idle 5:iowait 6:irq 7:softirq 8:steal
    // guest/guestnice are already counted in user/nice — skip them
    const quint64 user    = parts.value(1).toULongLong();
    const quint64 nice    = parts.value(2).toULongLong();
    const quint64 system  = parts.value(3).toULongLong();
    const quint64 idle    = parts.value(4).toULongLong();
    const quint64 iowait  = parts.value(5).toULongLong();
    const quint64 irq     = parts.value(6).toULongLong();
    const quint64 softirq = parts.value(7).toULongLong();
    const quint64 steal   = parts.value(8).toULongLong();

    outIdle   = idle + iowait;
    outKernel = system + irq + softirq;
    return user + nice + outKernel + outIdle + steal;
}

bool PerfDataProvider::sampleCpu()
{
    QFile f("/proc/stat");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    int coreIdx = 0;

    for (;;)
    {
        const QByteArray raw = f.readLine();
        if (raw.isNull())
            break;

        const QList<QByteArray> parts = raw.simplified().split(' ');
        if (parts.isEmpty())
            continue;

        const QByteArray key = parts.at(0);

        if (key == "cpu")
        {
            // Aggregate line
            quint64 idleAll = 0, kernelAll = 0;
            const quint64 total = parseCpuLine(parts, idleAll, kernelAll);

            const quint64 dTotal  = (total     > this->m_prevCpuTotal ) ? (total     - this->m_prevCpuTotal ) : 0;
            const quint64 dIdle   = (idleAll   > this->m_prevCpuIdle  ) ? (idleAll   - this->m_prevCpuIdle  ) : 0;
            const quint64 dKernel = (kernelAll > this->m_prevCpuKernel) ? (kernelAll - this->m_prevCpuKernel) : 0;

            double pct = 0.0, kpct = 0.0;
            if (dTotal > 0)
            {
                pct  = (1.0 - static_cast<double>(dIdle)   / static_cast<double>(dTotal)) * 100.0;
                kpct =        static_cast<double>(dKernel) / static_cast<double>(dTotal)  * 100.0;
            }

            this->m_prevCpuTotal  = total;
            this->m_prevCpuIdle   = idleAll;
            this->m_prevCpuKernel = kernelAll;
            appendHistory(this->m_cpuHistory,       pct);
            appendHistory(this->m_cpuKernelHistory, kpct);
        }
        else if (key.startsWith("cpu") && key.size() > 3)
        {
            // Per-core line: "cpu0", "cpu1", ...
            if (coreIdx >= this->m_cores.size())
                this->m_cores.resize(coreIdx + 1);

            CoreSample &c = this->m_cores[coreIdx];
            quint64 idleC = 0, kernelC = 0;
            const quint64 totalC = parseCpuLine(parts, idleC, kernelC);

            const quint64 dTotal  = (totalC  > c.prevTotal ) ? (totalC  - c.prevTotal ) : 0;
            const quint64 dIdle   = (idleC   > c.prevIdle  ) ? (idleC   - c.prevIdle  ) : 0;
            const quint64 dKernel = (kernelC > c.prevKernel) ? (kernelC - c.prevKernel) : 0;

            double pct = 0.0, kpct = 0.0;
            if (dTotal > 0)
            {
                pct  = (1.0 - static_cast<double>(dIdle)   / static_cast<double>(dTotal)) * 100.0;
                kpct =        static_cast<double>(dKernel) / static_cast<double>(dTotal)  * 100.0;
            }

            c.prevTotal  = totalC;
            c.prevIdle   = idleC;
            c.prevKernel = kernelC;
            appendHistory(c.history,       pct);
            appendHistory(c.kernelHistory, kpct);
            ++coreIdx;
        }
        else if (coreIdx > 0)
        {
            break;   // past cpu lines — stop reading for efficiency
        }
    }

    f.close();
    return true;
}

// ── Memory sampling ───────────────────────────────────────────────────────────

bool PerfDataProvider::sampleMemory()
{
    QFile f("/proc/meminfo");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    qint64 memTotal = 0, memAvail = 0, memFree = 0;
    qint64 buffers  = 0, cached   = 0, sReclaimable = 0, shmem = 0;
    qint64 dirty    = 0, writeback = 0;
    qint64 swapTotal = 0, swapFree = 0;

    for (;;)
    {
        const QByteArray line = f.readLine();
        if (line.isNull())
            break;

        const int        colon = line.indexOf(':');
        if (colon < 0)
            continue;
        const QByteArray key = line.left(colon).trimmed();

        // /proc/meminfo values are formatted like "   12345 kB".
        // Extract the first token explicitly and validate conversion.
        const QByteArray valueField = line.mid(colon + 1).simplified();
        const QByteArray numberToken = valueField.split(' ').value(0);
        bool ok = false;
        const qint64 val = numberToken.toLongLong(&ok);
        if (!ok)
            continue;

        if      (key == "MemTotal")     memTotal     = val;
        else if (key == "MemFree")      memFree      = val;
        else if (key == "MemAvailable") memAvail     = val;
        else if (key == "Buffers")      buffers      = val;
        else if (key == "Cached")       cached       = val;
        else if (key == "SReclaimable") sReclaimable = val;
        else if (key == "Shmem")        shmem        = val;
        else if (key == "Dirty")        dirty        = val;
        else if (key == "Writeback")    writeback    = val;
        else if (key == "SwapTotal")    swapTotal    = val;
        else if (key == "SwapFree")     swapFree     = val;
    }
    f.close();

    // htop formula: used = total - free - buffers - page_cache
    // where page_cache = Cached + SReclaimable - Shmem
    const qint64 pageCache = cached + sReclaimable - shmem;

    this->m_memTotalKb   = memTotal;
    this->m_memAvailKb   = memAvail;
    this->m_memFreeKb    = memFree;
    this->m_memBuffersKb = buffers;
    this->m_memDirtyKb   = dirty + writeback;
    // Full page cache including buffers (what we show in stats and composition bar)
    this->m_memCachedKb  = buffers + pageCache;
    // htop's "used" (processes' non-reclaimable footprint)
    this->m_memUsedKb    = qMax(0LL, memTotal - memFree - buffers - pageCache);
    this->m_swapTotalKb  = swapTotal;
    this->m_swapFreeKb   = swapFree;
    this->m_swapUsedKb   = qMax<qint64>(0, swapTotal - swapFree);

    // Graph tracks used / total (htop formula matches the green bar)
    const double frac = (memTotal > 0)
                        ? static_cast<double>(this->m_memUsedKb) / static_cast<double>(memTotal)
                        : 0.0;
    appendHistory(this->m_memHistory, frac * 100.0);

    if (this->m_swapSamplingEnabled)
    {
        const double swapFrac = (swapTotal > 0)
                                ? static_cast<double>(this->m_swapUsedKb) / static_cast<double>(swapTotal)
                                : 0.0;
        appendHistory(this->m_swapUsageHistory, swapFrac * 100.0);

        quint64 pswpin = 0;
        quint64 pswpout = 0;
        QFile vmf("/proc/vmstat");
        if (vmf.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            for (;;)
            {
                const QByteArray line = vmf.readLine();
                if (line.isNull())
                    break;
                const QList<QByteArray> parts = line.simplified().split(' ');
                if (parts.size() < 2)
                    continue;
                if (parts.at(0) == "pswpin")
                    pswpin = parts.at(1).toULongLong();
                else if (parts.at(0) == "pswpout")
                    pswpout = parts.at(1).toULongLong();
            }
            vmf.close();
        }

        if (!this->m_swapTimer.isValid())
            this->m_swapTimer.start();
        const qint64 nowMs = this->m_swapTimer.elapsed();
        const qint64 dtMs = (this->m_prevSwapSampleMs > 0) ? (nowMs - this->m_prevSwapSampleMs) : 0;
        this->m_prevSwapSampleMs = nowMs;

        if (dtMs > 0)
        {
            const quint64 dInPages = (pswpin >= this->m_prevSwapInPages) ? (pswpin - this->m_prevSwapInPages) : 0;
            const quint64 dOutPages = (pswpout >= this->m_prevSwapOutPages) ? (pswpout - this->m_prevSwapOutPages) : 0;
            const long pageSize = ::sysconf(_SC_PAGESIZE);
            const double bytesPerPage = (pageSize > 0) ? static_cast<double>(pageSize) : 4096.0;
            this->m_swapInBps = static_cast<double>(dInPages) * bytesPerPage * 1000.0 / static_cast<double>(dtMs);
            this->m_swapOutBps = static_cast<double>(dOutPages) * bytesPerPage * 1000.0 / static_cast<double>(dtMs);
        }
        else
        {
            this->m_swapInBps = 0.0;
            this->m_swapOutBps = 0.0;
        }

        this->m_prevSwapInPages = pswpin;
        this->m_prevSwapOutPages = pswpout;
        appendHistory(this->m_swapInHistory, this->m_swapInBps);
        appendHistory(this->m_swapOutHistory, this->m_swapOutBps);
    }
    return true;
}

// ── Disk sampling ─────────────────────────────────────────────────────────────

QString PerfDataProvider::readSysTextFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QString out = QString::fromUtf8(f.readAll()).trimmed();
    f.close();
    return out;
}

bool PerfDataProvider::shouldIgnoreBlockDevice(const QString &baseName)
{
    return baseName.startsWith("loop")
           || baseName.startsWith("ram")
           || baseName.startsWith("zram");
}

QSet<QString> PerfDataProvider::resolveBaseBlockDevices(const QString &devName)
{
    QSet<QString> out;
    if (devName.isEmpty() || devName == "." || devName == "..")
        return out;

    const QString sysPath = QString("/sys/class/block/%1").arg(devName);
    const QFileInfo fi(sysPath);
    if (!fi.exists())
        return out;

    // Device-mapper / md devices can have one or more "slaves".
    // If present, recurse into those and treat them as backing base devices.
    const QDir slavesDir(sysPath + "/slaves");
    const QStringList slaves = slavesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (!slaves.isEmpty())
    {
        for (const QString &slave : slaves)
            out.unite(resolveBaseBlockDevices(slave));
        return out;
    }

    // Partitions expose /sys/class/block/<name>/partition.
    // Walk to parent block device (e.g. sda1 -> sda, nvme0n1p2 -> nvme0n1).
    if (QFileInfo(sysPath + "/partition").exists())
    {
        QString parentName;
        const QString canonical = QFileInfo(sysPath).canonicalFilePath();
        if (!canonical.isEmpty())
        {
            QDir d(canonical);
            if (d.cdUp())
                parentName = d.dirName();
        }

        if ((parentName.isEmpty() || parentName == "." || parentName == "..") && !devName.isEmpty())
        {
            // Fallback when canonical parent lookup fails:
            // nvme0n1p2 -> nvme0n1, mmcblk0p1 -> mmcblk0, sda1 -> sda
            static const QRegularExpression partRe("^(.*?)(?:p)?\\d+$");
            const QRegularExpressionMatch m = partRe.match(devName);
            if (m.hasMatch())
                parentName = m.captured(1);
        }

        if (!parentName.isEmpty() && parentName != "." && parentName != ".." && parentName != devName)
            return resolveBaseBlockDevices(parentName);
    }

    out.insert(devName);
    return out;
}

void PerfDataProvider::refreshDisks(const QSet<QString> &measurableDevices)
{
    QSet<QString> rawDevNames;
    QHash<QString, QSet<QString>> mountPointsByRaw;
    QHash<QString, qint64> formattedByRaw;
    QHash<QString, bool> rawHasSwap;

    // Mounted filesystems
    QFile mf("/proc/self/mountinfo");
    if (mf.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        for (;;)
        {
            const QByteArray line = mf.readLine();
            if (line.isNull())
                break;

            const QList<QByteArray> parts = line.trimmed().split(' ');
            const int sepIdx = parts.indexOf("-");
            if (sepIdx < 0 || sepIdx + 2 >= parts.size())
                continue;

            const QString source = QString::fromUtf8(parts.at(sepIdx + 2));
            if (!source.startsWith("/dev/"))
                continue;
            const QString raw = QFileInfo(source).fileName();
            rawDevNames.insert(raw);

            const QString mountPoint = (parts.size() > 4) ? QString::fromUtf8(parts.at(4)) : QString();
            if (!mountPoint.isEmpty())
            {
                mountPointsByRaw[raw].insert(mountPoint);
                if (!formattedByRaw.contains(raw))
                {
                    struct statvfs vfs{};
                    if (::statvfs(mountPoint.toUtf8().constData(), &vfs) == 0)
                    {
                        formattedByRaw.insert(raw, static_cast<qint64>(vfs.f_blocks)
                                                    * static_cast<qint64>(vfs.f_frsize));
                    }
                }
            }
        }
        mf.close();
    }

    // Swap devices are in use even when not mounted.
    QFile sf("/proc/swaps");
    if (sf.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        bool header = true;
        for (;;)
        {
            const QByteArray line = sf.readLine();
            if (line.isNull())
                break;
            if (header)
            {
                header = false;
                continue;
            }
            const QList<QByteArray> parts = line.simplified().split(' ');
            if (parts.isEmpty())
                continue;
            const QString src = QString::fromUtf8(parts.at(0));
            if (!src.startsWith("/dev/"))
                continue;
            const QString raw = QFileInfo(src).fileName();
            rawDevNames.insert(raw);
            rawHasSwap.insert(raw, true);
        }
        sf.close();
    }

    QSet<QString> baseDevices;
    QHash<QString, bool> systemByBase;
    QHash<QString, bool> pageFileByBase;
    QHash<QString, qint64> formattedByBase;
    for (const QString &raw : rawDevNames)
    {
        const QSet<QString> bases = resolveBaseBlockDevices(raw);
        for (const QString &b : bases)
        {
            if (!shouldIgnoreBlockDevice(b) && measurableDevices.contains(b))
            {
                baseDevices.insert(b);
                for (const QString &mp : mountPointsByRaw.value(raw))
                {
                    if (mp == "/")
                        systemByBase[b] = true;
                }
                if (rawHasSwap.value(raw, false))
                    pageFileByBase[b] = true;
                formattedByBase[b] += formattedByRaw.value(raw, 0);
            }
        }
    }

    QStringList names = baseDevices.values();
    std::sort(names.begin(), names.end());

    // Preserve existing histories/counters for unchanged devices.
    QHash<QString, DiskSample> oldByName;
    oldByName.reserve(this->m_disks.size());
    for (const DiskSample &d : this->m_disks)
        oldByName.insert(d.name, d);

    QVector<DiskSample> rebuilt;
    rebuilt.reserve(names.size());
    for (const QString &name : names)
    {
        DiskSample d = oldByName.value(name);
        d.name = name;

        const QString model = readSysTextFile(QString("/sys/class/block/%1/device/model").arg(name));
        d.model = model.isEmpty() ? tr("Unknown device") : model;

        const QString rotational = readSysTextFile(QString("/sys/class/block/%1/queue/rotational").arg(name));
        if (rotational == "1")
            d.type = "HDD";
        else if (rotational == "0")
            d.type = "SSD";
        else
            d.type = tr("Unknown");

        const qint64 sizeSecs = readSysTextFile(QString("/sys/class/block/%1/size").arg(name)).toLongLong();
        d.capacityBytes = qMax<qint64>(0, sizeSecs) * 512LL;
        d.formattedBytes = qMax<qint64>(0, formattedByBase.value(name, 0));
        d.isSystemDisk = systemByBase.value(name, false);
        d.hasPageFile = pageFileByBase.value(name, false);

        rebuilt.append(d);
    }

    this->m_disks = rebuilt;
}

bool PerfDataProvider::sampleDisks()
{
    QFile f("/proc/diskstats");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    struct DiskCounters
    {
        quint64 readSectors  { 0 };
        quint64 writeSectors { 0 };
        quint64 ioMs         { 0 };
    };
    QHash<QString, DiskCounters> countersByName;
    QSet<QString> measurableDevices;

    for (;;)
    {
        const QByteArray line = f.readLine();
        if (line.isNull())
            break;
        const QList<QByteArray> parts = line.simplified().split(' ');
        if (parts.size() < 14)
            continue;

        DiskCounters c;
        c.readSectors  = parts.at(5).toULongLong();
        c.writeSectors = parts.at(9).toULongLong();
        c.ioMs         = parts.at(12).toULongLong();
        const QString devName = QString::fromUtf8(parts.at(2));
        countersByName.insert(devName, c);
        measurableDevices.insert(devName);
    }
    f.close();

    this->refreshDisks(measurableDevices);

    if (!this->m_diskTimer.isValid())
        this->m_diskTimer.start();

    const qint64 nowMs = this->m_diskTimer.elapsed();
    const qint64 dtMs = (this->m_prevDiskSampleMs > 0) ? (nowMs - this->m_prevDiskSampleMs) : 0;
    this->m_prevDiskSampleMs = nowMs;

    static constexpr double kSectorBytes = 512.0;

    for (DiskSample &d : this->m_disks)
    {
        const auto it = countersByName.constFind(d.name);
        if (it == countersByName.cend())
        {
            d.activePct = 0.0;
            d.readBps = 0.0;
            d.writeBps = 0.0;
            appendHistory(d.activeHistory, 0.0);
            appendHistory(d.readHistory, 0.0);
            appendHistory(d.writeHistory, 0.0);
            continue;
        }

        const DiskCounters c = it.value();
        if (dtMs <= 0)
        {
            d.prevReadSecs  = c.readSectors;
            d.prevWriteSecs = c.writeSectors;
            d.prevIoMs      = c.ioMs;
            appendHistory(d.activeHistory, 0.0);
            appendHistory(d.readHistory, 0.0);
            appendHistory(d.writeHistory, 0.0);
            continue;
        }

        const quint64 dReadSecs  = (c.readSectors  >= d.prevReadSecs ) ? (c.readSectors  - d.prevReadSecs ) : 0;
        const quint64 dWriteSecs = (c.writeSectors >= d.prevWriteSecs) ? (c.writeSectors - d.prevWriteSecs) : 0;
        const quint64 dIoMs      = (c.ioMs         >= d.prevIoMs     ) ? (c.ioMs         - d.prevIoMs     ) : 0;

        d.prevReadSecs  = c.readSectors;
        d.prevWriteSecs = c.writeSectors;
        d.prevIoMs      = c.ioMs;

        d.activePct = qBound(0.0,
                             static_cast<double>(dIoMs) * 100.0 / static_cast<double>(dtMs),
                             100.0);
        d.readBps  = static_cast<double>(dReadSecs)  * kSectorBytes * 1000.0 / static_cast<double>(dtMs);
        d.writeBps = static_cast<double>(dWriteSecs) * kSectorBytes * 1000.0 / static_cast<double>(dtMs);

        appendHistory(d.activeHistory, d.activePct);
        appendHistory(d.readHistory, d.readBps);
        appendHistory(d.writeHistory, d.writeBps);
    }

    return true;
}

// ── Network sampling ─────────────────────────────────────────────────────────

bool PerfDataProvider::isActiveNetworkInterface(const QString &name)
{
    if (name.isEmpty() || name == "lo")
        return false;

    const QString operState = readSysTextFile(QString("/sys/class/net/%1/operstate").arg(name)).toLower();
    if (operState != "up")
        return false;

    const QString carrierStr = readSysTextFile(QString("/sys/class/net/%1/carrier").arg(name));
    if (!carrierStr.isEmpty() && carrierStr != "1")
        return false;

    return true;
}

QString PerfDataProvider::networkTypeFromArpType(int arpType)
{
    // ARPHRD_ETHER (1), ARPHRD_LOOPBACK (772), ARPHRD_IEEE80211* (801+)
    if (arpType == 1)
        return "Ethernet";
    if (arpType == 772)
        return "Loopback";
    if (arpType >= 801 && arpType <= 804)
        return "Wi-Fi";
    return tr("Network");
}

int PerfDataProvider::readLinkSpeedMbps(const QString &name)
{
    bool ok = false;
    const int speed = readSysTextFile(QString("/sys/class/net/%1/speed").arg(name)).toInt(&ok);
    if (!ok || speed <= 0)
        return 0;
    return speed;
}

bool PerfDataProvider::sampleNetworks()
{
    QFile f("/proc/net/dev");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    struct NetCounters
    {
        quint64 rxBytes { 0 };
        quint64 txBytes { 0 };
    };
    QHash<QString, NetCounters> countersByName;
    QStringList activeNames;

    int lineNo = 0;
    for (;;)
    {
        const QByteArray line = f.readLine();
        if (line.isNull())
            break;
        ++lineNo;
        if (lineNo <= 2)
            continue; // headers

        const int colon = line.indexOf(':');
        if (colon < 0)
            continue;

        const QString ifName = QString::fromUtf8(line.left(colon)).trimmed();
        if (!isActiveNetworkInterface(ifName))
            continue;

        const QList<QByteArray> fields = line.mid(colon + 1).simplified().split(' ');
        if (fields.size() < 9)
            continue;

        NetCounters c;
        c.rxBytes = fields.at(0).toULongLong();
        c.txBytes = fields.at(8).toULongLong();
        countersByName.insert(ifName, c);
        activeNames.append(ifName);
    }
    f.close();

    std::sort(activeNames.begin(), activeNames.end());

    // Best-effort interface metadata (IP addresses + type) from getifaddrs.
    struct IfAddrInfo
    {
        QString ipv4;
        QString ipv6;
    };
    QHash<QString, IfAddrInfo> ifaddrByName;
    struct ifaddrs *ifaddr = nullptr;
    if (::getifaddrs(&ifaddr) == 0)
    {
        for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
        {
            if (!ifa->ifa_name || !ifa->ifa_addr)
                continue;
            const QString name = QString::fromUtf8(ifa->ifa_name);
            if (!countersByName.contains(name))
                continue;

            char host[NI_MAXHOST] = {};
            const int fam = ifa->ifa_addr->sa_family;
            if (fam != AF_INET && fam != AF_INET6)
                continue;

            const socklen_t addrLen = (fam == AF_INET)
                                      ? static_cast<socklen_t>(sizeof(sockaddr_in))
                                      : static_cast<socklen_t>(sizeof(sockaddr_in6));
            if (::getnameinfo(ifa->ifa_addr, addrLen,
                              host, sizeof(host),
                              nullptr, 0,
                              NI_NUMERICHOST) != 0)
            {
                continue;
            }

            IfAddrInfo &info = ifaddrByName[name];
            if (fam == AF_INET && info.ipv4.isEmpty())
                info.ipv4 = QString::fromLatin1(host);
            else if (fam == AF_INET6 && info.ipv6.isEmpty())
                info.ipv6 = QString::fromLatin1(host);
        }
        ::freeifaddrs(ifaddr);
    }

    QHash<QString, NetworkSample> oldByName;
    oldByName.reserve(this->m_networks.size());
    for (const NetworkSample &n : this->m_networks)
        oldByName.insert(n.name, n);

    QVector<NetworkSample> rebuilt;
    rebuilt.reserve(activeNames.size());
    for (const QString &name : activeNames)
    {
        NetworkSample n = oldByName.value(name);
        n.name = name;

        const int arpType = readSysTextFile(QString("/sys/class/net/%1/type").arg(name)).toInt();
        n.type = networkTypeFromArpType(arpType);
        n.linkSpeedMbps = readLinkSpeedMbps(name);
        n.ipv4 = ifaddrByName.value(name).ipv4;
        n.ipv6 = ifaddrByName.value(name).ipv6;
        rebuilt.append(n);
    }

    this->m_networks = rebuilt;

    if (!this->m_netTimer.isValid())
        this->m_netTimer.start();

    const qint64 nowMs = this->m_netTimer.elapsed();
    const qint64 dtMs = (this->m_prevNetSampleMs > 0) ? (nowMs - this->m_prevNetSampleMs) : 0;
    this->m_prevNetSampleMs = nowMs;

    for (NetworkSample &n : this->m_networks)
    {
        const auto it = countersByName.constFind(n.name);
        if (it == countersByName.cend())
        {
            n.rxBps = 0.0;
            n.txBps = 0.0;
            appendHistory(n.rxHistory, 0.0);
            appendHistory(n.txHistory, 0.0);
            continue;
        }

        const NetCounters c = it.value();
        if (dtMs <= 0)
        {
            n.prevRxBytes = c.rxBytes;
            n.prevTxBytes = c.txBytes;
            appendHistory(n.rxHistory, 0.0);
            appendHistory(n.txHistory, 0.0);
            continue;
        }

        const quint64 dRx = (c.rxBytes >= n.prevRxBytes) ? (c.rxBytes - n.prevRxBytes) : 0;
        const quint64 dTx = (c.txBytes >= n.prevTxBytes) ? (c.txBytes - n.prevTxBytes) : 0;
        n.prevRxBytes = c.rxBytes;
        n.prevTxBytes = c.txBytes;

        n.rxBps = static_cast<double>(dRx) * 1000.0 / static_cast<double>(dtMs);
        n.txBps = static_cast<double>(dTx) * 1000.0 / static_cast<double>(dtMs);
        appendHistory(n.rxHistory, n.rxBps);
        appendHistory(n.txHistory, n.txBps);
    }

    return true;
}

// ── GPU sampling ──────────────────────────────────────────────────────────────

void PerfDataProvider::detectGpuBackends()
{
    this->m_hasNvml = false;
    this->m_nvmlLibHandle = nullptr;

    this->m_nvmlLibHandle = ::dlopen("libnvidia-ml.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (!this->m_nvmlLibHandle)
        this->m_nvmlLibHandle = ::dlopen("libnvidia-ml.so", RTLD_LAZY | RTLD_LOCAL);
    if (!this->m_nvmlLibHandle)
        return;

    pNvmlInitV2 = reinterpret_cast<FnNvmlInitV2>(::dlsym(this->m_nvmlLibHandle, "nvmlInit_v2"));
    pNvmlShutdown = reinterpret_cast<FnNvmlShutdown>(::dlsym(this->m_nvmlLibHandle, "nvmlShutdown"));
    pNvmlSystemGetDriverVersion = reinterpret_cast<FnNvmlSystemGetDriverVersion>(
                ::dlsym(this->m_nvmlLibHandle, "nvmlSystemGetDriverVersion"));
    pNvmlDeviceGetCountV2 = reinterpret_cast<FnNvmlDeviceGetCountV2>(
                ::dlsym(this->m_nvmlLibHandle, "nvmlDeviceGetCount_v2"));
    pNvmlDeviceGetHandleByIndexV2 = reinterpret_cast<FnNvmlDeviceGetHandleByIndexV2>(
                ::dlsym(this->m_nvmlLibHandle, "nvmlDeviceGetHandleByIndex_v2"));
    pNvmlDeviceGetName = reinterpret_cast<FnNvmlDeviceGetName>(
                ::dlsym(this->m_nvmlLibHandle, "nvmlDeviceGetName"));
    pNvmlDeviceGetUUID = reinterpret_cast<FnNvmlDeviceGetUUID>(
                ::dlsym(this->m_nvmlLibHandle, "nvmlDeviceGetUUID"));
    pNvmlDeviceGetUtilizationRates = reinterpret_cast<FnNvmlDeviceGetUtilizationRates>(
                ::dlsym(this->m_nvmlLibHandle, "nvmlDeviceGetUtilizationRates"));
    pNvmlDeviceGetMemoryInfo = reinterpret_cast<FnNvmlDeviceGetMemoryInfo>(
                ::dlsym(this->m_nvmlLibHandle, "nvmlDeviceGetMemoryInfo"));
    pNvmlDeviceGetEncoderUtilization = reinterpret_cast<FnNvmlDeviceGetEncoderUtilization>(
                ::dlsym(this->m_nvmlLibHandle, "nvmlDeviceGetEncoderUtilization"));
    pNvmlDeviceGetDecoderUtilization = reinterpret_cast<FnNvmlDeviceGetDecoderUtilization>(
                ::dlsym(this->m_nvmlLibHandle, "nvmlDeviceGetDecoderUtilization"));
    pNvmlDeviceGetPcieThroughput = reinterpret_cast<FnNvmlDeviceGetPcieThroughput>(
                ::dlsym(this->m_nvmlLibHandle, "nvmlDeviceGetPcieThroughput"));
    pNvmlDeviceGetTemperature = reinterpret_cast<FnNvmlDeviceGetTemperature>(
                ::dlsym(this->m_nvmlLibHandle, "nvmlDeviceGetTemperature"));

    if (!pNvmlInitV2 || !pNvmlShutdown || !pNvmlDeviceGetCountV2
        || !pNvmlDeviceGetHandleByIndexV2 || !pNvmlDeviceGetName
        || !pNvmlDeviceGetUUID || !pNvmlDeviceGetUtilizationRates
        || !pNvmlDeviceGetMemoryInfo)
    {
        this->unloadGpuBackends();
        return;
    }

    if (pNvmlInitV2() != NVML_SUCCESS)
    {
        this->unloadGpuBackends();
        return;
    }

    unsigned int count = 0;
    if (pNvmlDeviceGetCountV2(&count) != NVML_SUCCESS || count == 0)
    {
        this->unloadGpuBackends();
        return;
    }

    this->m_hasNvml = true;
}

bool PerfDataProvider::sampleGpus()
{
    if (this->m_hasNvml)
        return this->sampleNvml();

    this->m_gpus.clear();
    return true;
}

void PerfDataProvider::unloadGpuBackends()
{
    if (this->m_hasNvml && pNvmlShutdown)
        pNvmlShutdown();

    this->m_hasNvml = false;

    pNvmlInitV2 = nullptr;
    pNvmlShutdown = nullptr;
    pNvmlSystemGetDriverVersion = nullptr;
    pNvmlDeviceGetCountV2 = nullptr;
    pNvmlDeviceGetHandleByIndexV2 = nullptr;
    pNvmlDeviceGetName = nullptr;
    pNvmlDeviceGetUUID = nullptr;
    pNvmlDeviceGetUtilizationRates = nullptr;
    pNvmlDeviceGetMemoryInfo = nullptr;
    pNvmlDeviceGetEncoderUtilization = nullptr;
    pNvmlDeviceGetDecoderUtilization = nullptr;
    pNvmlDeviceGetPcieThroughput = nullptr;
    pNvmlDeviceGetTemperature = nullptr;

    if (this->m_nvmlLibHandle)
    {
        ::dlclose(this->m_nvmlLibHandle);
        this->m_nvmlLibHandle = nullptr;
    }
}

bool PerfDataProvider::sampleNvml()
{
    if (!this->m_hasNvml || !pNvmlDeviceGetCountV2 || !pNvmlDeviceGetHandleByIndexV2)
        return false;

    unsigned int count = 0;
    if (pNvmlDeviceGetCountV2(&count) != NVML_SUCCESS)
        return false;

    char driverVer[96] = {};
    if (pNvmlSystemGetDriverVersion)
        pNvmlSystemGetDriverVersion(driverVer, sizeof(driverVer));
    const QString driverVersion = QString::fromLatin1(driverVer).trimmed();

    QHash<QString, GpuSample> oldById;
    oldById.reserve(this->m_gpus.size());
    for (const GpuSample &g : this->m_gpus)
        oldById.insert(g.id, g);

    QVector<GpuSample> rebuilt;
    rebuilt.reserve(static_cast<int>(count));

    for (unsigned int i = 0; i < count; ++i)
    {
        static constexpr unsigned int kNvmlPcieTxBytes = 0;
        static constexpr unsigned int kNvmlPcieRxBytes = 1;
        static constexpr unsigned int kNvmlTemperatureGpu = 0;

        NvmlDevice dev = nullptr;
        if (pNvmlDeviceGetHandleByIndexV2(i, &dev) != NVML_SUCCESS || !dev)
            continue;

        char nameBuf[128] = {};
        char uuidBuf[96] = {};
        pNvmlDeviceGetName(dev, nameBuf, sizeof(nameBuf));
        pNvmlDeviceGetUUID(dev, uuidBuf, sizeof(uuidBuf));

        const QString name = QString::fromLatin1(nameBuf).trimmed();
        QString id = QString::fromLatin1(uuidBuf).trimmed();
        if (id.isEmpty())
            id = QString("gpu-%1").arg(i);

        NvmlUtilization util{};
        NvmlMemory mem{};
        const bool hasUtil = (pNvmlDeviceGetUtilizationRates(dev, &util) == NVML_SUCCESS);
        const bool hasMem = (pNvmlDeviceGetMemoryInfo(dev, &mem) == NVML_SUCCESS);
        unsigned int txKBps = 0;
        unsigned int rxKBps = 0;
        const bool hasTx = pNvmlDeviceGetPcieThroughput
                           && (pNvmlDeviceGetPcieThroughput(dev, kNvmlPcieTxBytes, &txKBps) == NVML_SUCCESS);
        const bool hasRx = pNvmlDeviceGetPcieThroughput
                           && (pNvmlDeviceGetPcieThroughput(dev, kNvmlPcieRxBytes, &rxKBps) == NVML_SUCCESS);
        unsigned int tempC = 0;
        const bool hasTemp = pNvmlDeviceGetTemperature
                             && (pNvmlDeviceGetTemperature(dev, kNvmlTemperatureGpu, &tempC) == NVML_SUCCESS);

        GpuSample g = oldById.value(id);
        g.id = id;
        g.name = name;
        g.driverVersion = driverVersion;
        g.backend = "NVML";
        g.utilPct = hasUtil ? qBound(0.0, static_cast<double>(util.gpu), 100.0) : 0.0;
        g.temperatureC = hasTemp ? static_cast<int>(tempC) : -1;
        g.memUsedMiB = hasMem ? static_cast<qint64>(mem.used / (1024ULL * 1024ULL)) : 0;
        g.memTotalMiB = hasMem ? static_cast<qint64>(mem.total / (1024ULL * 1024ULL)) : 0;
        g.copyTxBps = hasTx ? static_cast<double>(txKBps) * 1024.0 : 0.0;
        g.copyRxBps = hasRx ? static_cast<double>(rxKBps) * 1024.0 : 0.0;
        appendHistory(g.utilHistory, g.utilPct);
        const double memPct = (g.memTotalMiB > 0)
                              ? (static_cast<double>(g.memUsedMiB) / static_cast<double>(g.memTotalMiB)) * 100.0
                              : 0.0;
        appendHistory(g.memUsageHistory, memPct);
        appendHistory(g.copyTxHistory, g.copyTxBps);
        appendHistory(g.copyRxHistory, g.copyRxBps);

        QHash<QString, GpuEngineSample> oldEngines;
        for (const GpuEngineSample &e : g.engines)
            oldEngines.insert(e.key, e);

        QVector<GpuEngineSample> engines;
        auto addEngine = [&](const QString &key, const QString &label, double pct)
        {
            GpuEngineSample eng = oldEngines.value(key);
            eng.key = key;
            eng.label = label;
            eng.pct = qBound(0.0, pct, 100.0);
            appendHistory(eng.history, eng.pct);
            engines.append(eng);
        };

        if (hasUtil)
        {
            addEngine("3d", "3D", util.gpu);
            addEngine("cuda", "CUDA", util.gpu);
            addEngine("copy", "Copy", util.memory);
        }
        if (pNvmlDeviceGetEncoderUtilization)
        {
            unsigned int enc = 0, sampling = 0;
            if (pNvmlDeviceGetEncoderUtilization(dev, &enc, &sampling) == NVML_SUCCESS)
                addEngine("video-encode", "Video Encode", enc);
        }
        if (pNvmlDeviceGetDecoderUtilization)
        {
            unsigned int dec = 0, sampling = 0;
            if (pNvmlDeviceGetDecoderUtilization(dev, &dec, &sampling) == NVML_SUCCESS)
                addEngine("video-decode", "Video Decode", dec);
        }

        g.engines = engines;
        rebuilt.append(g);
    }

    this->m_gpus = rebuilt;
    return true;
}

double PerfDataProvider::parsePercentField(const QString &field)
{
    static const QRegularExpression re("([0-9]+(?:\\.[0-9]+)?)");
    const QRegularExpressionMatch m = re.match(field);
    if (!m.hasMatch())
        return -1.0;
    bool ok = false;
    const double v = m.captured(1).toDouble(&ok);
    return ok ? v : -1.0;
}

qint64 PerfDataProvider::parseMiBField(const QString &field)
{
    static const QRegularExpression re("([0-9]+)");
    const QRegularExpressionMatch m = re.match(field);
    if (!m.hasMatch())
        return -1;
    bool ok = false;
    const qint64 v = m.captured(1).toLongLong(&ok);
    return ok ? v : -1;
}

// ── Process / thread counts ───────────────────────────────────────────────────

void PerfDataProvider::sampleProcessStats()
{
    int procs = 0, threads = 0;
    const QDir procDir("/proc");
    for (const QString &entry : procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        bool ok;
        entry.toInt(&ok);
        if (!ok)
            continue;
        ++procs;
        // Each thread appears as a subdirectory entry under /proc/<pid>/task/
        const QDir taskDir(QString("/proc/%1/task").arg(entry));
        // count() includes "." and ".." so subtract 2
        threads += static_cast<int>(taskDir.count()) - 2;
    }
    this->m_processCount = procs;
    this->m_threadCount  = qMax(0, threads);
}

// ── CPU metadata ──────────────────────────────────────────────────────────────

void PerfDataProvider::readCpuMetadata()
{
    this->m_cpuLogicalCount = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));

    QFile f("/proc/cpuinfo");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    bool gotModel = false, gotBase = false, gotFlags = false;
    bool hasHypervisorFlag = false;
    for (;;)
    {
        const QByteArray line = f.readLine();
        if (line.isNull())
            break;

        const int        colon = line.indexOf(':');
        if (colon < 0)
            continue;
        const QByteArray key = line.left(colon).trimmed();
        const QByteArray val = line.mid(colon + 1).trimmed();

        if (!gotModel && key == "model name")
        {
            this->m_cpuModelName = QString::fromUtf8(val);
            gotModel = true;
        } else if (!gotBase && key == "cpu MHz")
        {
            this->m_cpuBaseMhz = val.toDouble();
            gotBase = true;
        } else if (!gotFlags && key == "flags")
        {
            hasHypervisorFlag = val.contains("hypervisor");
            gotFlags = true;
        }

        if (gotModel && gotBase && gotFlags)
            break;
    }
    f.close();

    this->m_cpuIsVirtualMachine = hasHypervisorFlag;
    this->readCurrentFreq();
}

void PerfDataProvider::readCurrentFreq()
{
    const int coreCount = qMax(this->m_cores.size(), this->m_cpuLogicalCount);
    QVector<double> coreMhz(coreCount, 0.0);

    // Parse per-core live frequencies from /proc/cpuinfo.
    int currentProcessor = -1;
    QFile f("/proc/cpuinfo");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        for (;;)
        {
            const QByteArray line = f.readLine();
            if (line.isNull())
                break;

            const int colon = line.indexOf(':');
            if (colon < 0)
                continue;

            const QByteArray key = line.left(colon).trimmed();
            const QByteArray val = line.mid(colon + 1).trimmed();
            if (key == "processor")
            {
                currentProcessor = val.toInt();
            }
            else if (key == "cpu MHz" && currentProcessor >= 0 && currentProcessor < coreMhz.size())
            {
                coreMhz[currentProcessor] = val.toDouble();
            }
        }
        f.close();
    }

    double sumMhz = 0.0;
    int countMhz = 0;
    for (double mhz : coreMhz)
    {
        if (mhz > 0.0)
        {
            sumMhz += mhz;
            ++countMhz;
        }
    }

    // Fallback when per-core values are unavailable.
    if (countMhz == 0)
    {
        QFile sf("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
        if (sf.open(QIODevice::ReadOnly))
        {
            const double kHz = sf.readAll().trimmed().toDouble();
            sf.close();
            if (kHz > 0.0)
            {
                const double mhz = kHz / 1000.0;
                if (!coreMhz.isEmpty())
                    coreMhz[0] = mhz;
                this->m_cpuCurrentMhz = mhz;
                this->m_coreCurrentMhz = coreMhz;
                return;
            }
        }
        this->m_cpuCurrentMhz = 0.0;
        this->m_coreCurrentMhz = coreMhz;
        return;
    }

    this->m_cpuCurrentMhz = sumMhz / static_cast<double>(countMhz);
    this->m_coreCurrentMhz = coreMhz;
}

void PerfDataProvider::readHardwareMetadata()
{
    // Best-effort VM detection from DMI strings.
    const QString dmiVendor  = readSysTextFile("/sys/devices/virtual/dmi/id/sys_vendor").toLower();
    const QString dmiProduct = readSysTextFile("/sys/devices/virtual/dmi/id/product_name").toLower();
    const QString dmiBoard   = readSysTextFile("/sys/devices/virtual/dmi/id/board_vendor").toLower();
    const QString dmiBios    = readSysTextFile("/sys/devices/virtual/dmi/id/bios_vendor").toLower();
    const QString dmiAll = dmiVendor + " " + dmiProduct + " " + dmiBoard + " " + dmiBios;

    struct VmMarker { const char *needle; const char *label; };
    static const VmMarker kVmMarkers[] = {
        { "kvm",        "KVM" },
        { "qemu",       "QEMU" },
        { "vmware",     "VMware" },
        { "virtualbox", "VirtualBox" },
        { "microsoft",  "Hyper-V" },
        { "hyper-v",    "Hyper-V" },
        { "xen",        "Xen" },
        { "bhyve",      "bhyve" },
        { "parallels",  "Parallels" }
    };

    for (const VmMarker &m : kVmMarkers)
    {
        if (dmiAll.contains(m.needle))
        {
            this->m_cpuIsVirtualMachine = true;
            this->m_cpuVmVendor = QString::fromLatin1(m.label);
            break;
        }
    }

    // Best-effort DIMM population and memory speed from SMBIOS type 17.
    const QDir entriesDir("/sys/firmware/dmi/entries");
    const QStringList type17 = entriesDir.entryList(QStringList() << "17-*", QDir::Dirs | QDir::NoDotAndDotDot);
    int slotsTotal = 0;
    int slotsUsed = 0;
    int speedMtps = 0;

    for (const QString &entry : type17)
    {
        QFile rawFile(entriesDir.absoluteFilePath(entry + "/raw"));
        if (!rawFile.open(QIODevice::ReadOnly))
            continue;

        const QByteArray raw = rawFile.readAll();
        rawFile.close();
        if (raw.size() < 0x17)
            continue;

        ++slotsTotal;

        const int structLen = static_cast<unsigned char>(raw.at(1));
        const quint16 sizeField = readLe16(raw, 12);

        qint64 sizeMb = 0;
        if (sizeField == 0 || sizeField == 0xFFFF)
            sizeMb = 0;
        else if (sizeField == 0x7FFF && structLen >= 0x20)
            sizeMb = static_cast<qint64>(readLe32(raw, 28));
        else if (sizeField & 0x8000)
            sizeMb = static_cast<qint64>(sizeField & 0x7FFF) / 1024; // value is in KiB
        else
            sizeMb = static_cast<qint64>(sizeField); // value is in MiB

        if (sizeMb > 0)
        {
            ++slotsUsed;
            const int speed = static_cast<int>(readLe16(raw, 21));
            int configured = 0;
            if (structLen >= 0x22)
                configured = static_cast<int>(readLe16(raw, 32));
            speedMtps = qMax(speedMtps, configured > 0 ? configured : speed);
        }
    }

    this->m_memDimmSlotsTotal = slotsTotal;
    this->m_memDimmSlotsUsed  = slotsUsed;
    this->m_memSpeedMtps      = speedMtps;
}

void PerfDataProvider::detectCpuTemperatureSensor()
{
    this->m_cpuTempInputPath.clear();
    this->m_cpuTemperatureC = -1;

    const QDir hwmonRoot("/sys/class/hwmon");
    const QStringList hwmons = hwmonRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    struct Candidate
    {
        int score { -1 };
        QString inputPath;
    };
    Candidate best;

    static const QStringList kPreferredChipNames
    {
        "coretemp", "k10temp", "zenpower", "cpu", "soc_thermal", "x86_pkg_temp"
    };
    static const QStringList kPreferredTempLabels
    {
        "package", "tdie", "tctl", "cpu", "core"
    };

    for (const QString &hwmon : hwmons)
    {
        const QString basePath = QString("/sys/class/hwmon/%1").arg(hwmon);
        const QString chipName = readSysTextFile(basePath + "/name");
        const bool chipLooksCpu = textContainsAnyToken(chipName, kPreferredChipNames);

        QDir dir(basePath);
        const QStringList tempInputs = dir.entryList(QStringList() << "temp*_input", QDir::Files, QDir::Name);
        for (const QString &input : tempInputs)
        {
            const QString idx = input.mid(4, input.size() - 10); // temp + N + _input
            const QString label = readSysTextFile(basePath + "/temp" + idx + "_label");
            bool ok = false;
            const int milliC = readSysTextFile(basePath + "/" + input).toInt(&ok);
            if (!ok || milliC <= 0)
                continue;

            int score = 0;
            if (chipLooksCpu)
                score += 20;
            if (textContainsAnyToken(label, kPreferredTempLabels))
                score += 10;
            if (label.toLower().contains("package") || label.toLower().contains("tdie"))
                score += 10;

            // Keep plausible CPU temperatures, but allow warm systems.
            if (milliC >= 10000 && milliC <= 120000)
                score += 1;

            if (score > best.score)
            {
                best.score = score;
                best.inputPath = basePath + "/" + input;
            }
        }
    }

    if (best.score >= 0)
        this->m_cpuTempInputPath = best.inputPath;
}

void PerfDataProvider::sampleCpuTemperature()
{
    if (this->m_cpuTempInputPath.isEmpty())
    {
        this->m_cpuTemperatureC = -1;
        return;
    }

    bool ok = false;
    const int milliC = readSysTextFile(this->m_cpuTempInputPath).toInt(&ok);
    if (!ok || milliC <= 0)
    {
        this->m_cpuTemperatureC = -1;
        return;
    }

    this->m_cpuTemperatureC = milliC / 1000;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

quint16 PerfDataProvider::readLe16(const QByteArray &raw, int off)
{
    if (off < 0 || off + 1 >= raw.size())
        return 0;
    const quint16 b0 = static_cast<unsigned char>(raw.at(off));
    const quint16 b1 = static_cast<unsigned char>(raw.at(off + 1));
    return static_cast<quint16>(b0 | (b1 << 8));
}

quint32 PerfDataProvider::readLe32(const QByteArray &raw, int off)
{
    if (off < 0 || off + 3 >= raw.size())
        return 0;
    const quint32 b0 = static_cast<unsigned char>(raw.at(off));
    const quint32 b1 = static_cast<unsigned char>(raw.at(off + 1));
    const quint32 b2 = static_cast<unsigned char>(raw.at(off + 2));
    const quint32 b3 = static_cast<unsigned char>(raw.at(off + 3));
    return static_cast<quint32>(b0 | (b1 << 8) | (b2 << 16) | (b3 << 24));
}

// static
void PerfDataProvider::appendHistory(QVector<double> &vec, double val)
{
    vec.append(val);
    while (vec.size() > HISTORY_SIZE)
        vec.removeFirst();
}
