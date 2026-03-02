#include "perfdataprovider.h"

#include <QFile>

namespace Perf
{

PerfDataProvider::PerfDataProvider(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    connect(this->m_timer, &QTimer::timeout, this, &PerfDataProvider::onTimer);
    this->m_timer->start(1000);

    // Prime the CPU baseline so the first real sample has a delta to work from
    this->sampleCpu();
    this->sampleMemory();
}

void PerfDataProvider::setInterval(int ms)
{
    this->m_timer->setInterval(ms);
}

double PerfDataProvider::memFraction() const
{
    if (this->m_memTotalKb <= 0)
        return 0.0;
    return static_cast<double>(this->m_memUsedKb)
           / static_cast<double>(this->m_memTotalKb);
}

// ── Private slots ─────────────────────────────────────────────────────────────

void PerfDataProvider::onTimer()
{
    this->sampleCpu();
    this->sampleMemory();
    emit this->updated();
}

// ── Sampling ──────────────────────────────────────────────────────────────────

bool PerfDataProvider::sampleCpu()
{
    // /proc/stat first line: "cpu user nice system idle iowait irq softirq steal guest guestnice"
    QFile f("/proc/stat");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    const QByteArray line = f.readLine();
    f.close();

    const QList<QByteArray> parts = line.simplified().split(' ');
    // parts[0] = "cpu"; fields 1..10 follow
    if (parts.size() < 6)
        return false;

    // guest/guestnice (indices 9,10) are already included in user/nice — skip them
    quint64 user     = parts.value(1).toULongLong();
    quint64 nice     = parts.value(2).toULongLong();
    quint64 system   = parts.value(3).toULongLong();
    quint64 idle     = parts.value(4).toULongLong();
    quint64 iowait   = parts.value(5).toULongLong();
    quint64 irq      = parts.value(6).toULongLong();
    quint64 softirq  = parts.value(7).toULongLong();
    quint64 steal    = parts.value(8).toULongLong();

    const quint64 idleAll  = idle + iowait;
    const quint64 total    = user + nice + system + idleAll + irq + softirq + steal;

    const quint64 deltaTotal = (total > this->m_prevCpuTotal)
                               ? (total - this->m_prevCpuTotal) : 0;
    const quint64 deltaIdle  = (idleAll > this->m_prevCpuIdle)
                               ? (idleAll - this->m_prevCpuIdle) : 0;

    double pct = 0.0;
    if (deltaTotal > 0)
        pct = (1.0 - static_cast<double>(deltaIdle) / static_cast<double>(deltaTotal)) * 100.0;

    this->m_prevCpuTotal = total;
    this->m_prevCpuIdle  = idleAll;
    appendHistory(this->m_cpuHistory, pct);
    return true;
}

bool PerfDataProvider::sampleMemory()
{
    // Parse a subset of /proc/meminfo
    QFile f("/proc/meminfo");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    qint64 memTotal   = 0;
    qint64 memAvail   = 0;
    qint64 memFree    = 0;
    qint64 buffers    = 0;
    qint64 cached     = 0;
    qint64 sReclaimable = 0;
    qint64 shmem      = 0;

    while (!f.atEnd())
    {
        const QByteArray line = f.readLine();
        // Lines look like: "MemTotal:       16384000 kB"
        const int colon = line.indexOf(':');
        if (colon < 0)
            continue;
        const QByteArray key = line.left(colon).trimmed();
        const qint64     val = line.mid(colon + 1).trimmed().split(' ').value(0).toLongLong();

        if      (key == "MemTotal")      memTotal      = val;
        else if (key == "MemFree")       memFree       = val;
        else if (key == "MemAvailable")  memAvail      = val;
        else if (key == "Buffers")       buffers       = val;
        else if (key == "Cached")        cached        = val;
        else if (key == "SReclaimable")  sReclaimable  = val;
        else if (key == "Shmem")         shmem         = val;
    }
    f.close();

    this->m_memTotalKb   = memTotal;
    this->m_memAvailKb   = memAvail;
    this->m_memBuffersKb = buffers;
    // "Cached" in /proc/meminfo does not include SReclaimable; add it,
    // then subtract Shmem which is already counted in MemFree column
    this->m_memCachedKb  = cached + sReclaimable - shmem;

    // Used = Total - Available  (matches output of `free`)
    if (memAvail > 0)
        this->m_memUsedKb = qMax(0LL, memTotal - memAvail);
    else
        this->m_memUsedKb = qMax(0LL, memTotal - memFree - buffers - cached);

    const double frac = (memTotal > 0)
                        ? static_cast<double>(this->m_memUsedKb) / static_cast<double>(memTotal)
                        : 0.0;
    appendHistory(this->m_memHistory, frac * 100.0);
    return true;
}

// static
void PerfDataProvider::appendHistory(QVector<double> &vec, double val)
{
    vec.append(val);
    while (vec.size() > HISTORY_SIZE)
        vec.removeFirst();
}

} // namespace Perf
