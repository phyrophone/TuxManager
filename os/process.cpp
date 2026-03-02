#include "process.h"

#include <QDir>
#include <QFile>

#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

namespace Os
{

// ── Helpers ──────────────────────────────────────────────────────────────────

QString Process::stateString(char state)
{
    switch (state)
    {
        case 'R': return "Running";
        case 'S': return "Sleeping";
        case 'D': return "Disk sleep";
        case 'Z': return "Zombie";
        case 'T': return "Stopped";
        case 't': return "Tracing stop";
        case 'I': return "Idle";
        case 'X': return "Dead";
        default:  return QString(state);
    }
}

// ── Private: load a single process ───────────────────────────────────────────

bool Process::loadOne(pid_t pid, Process &out)
{
    out.pid = pid;

    // ── /proc/pid/stat ──────────────────────────────────────────────────────
    // Format: pid (comm) state ppid pgrp session tty_nr tpgid flags
    //         minflt cminflt majflt cmajflt utime stime cutime cstime
    //         priority nice num_threads itrealvalue starttime vsize rss ...
    //
    // comm can contain spaces and parentheses, so we anchor on the last ')'.

    QFile statFile(QString("/proc/%1/stat").arg(pid));
    if (!statFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    const QByteArray statData = statFile.readAll().trimmed();
    statFile.close();

    const int commStart = statData.indexOf('(');
    const int commEnd   = statData.lastIndexOf(')');
    if (commStart < 0 || commEnd < 0 || commEnd <= commStart)
        return false;

    out.name = QString::fromUtf8(statData.mid(commStart + 1, commEnd - commStart - 1));

    // Fields after ") " (0-indexed):
    //  0:state 1:ppid 2:pgrp 3:session 4:tty_nr 5:tpgid 6:flags
    //  7:minflt 8:cminflt 9:majflt 10:cmajflt
    //  11:utime 12:stime 13:cutime 14:cstime
    //  15:priority 16:nice 17:num_threads 18:itrealvalue
    //  19:starttime 20:vsize 21:rss
    const QList<QByteArray> f = statData.mid(commEnd + 2).split(' ');
    if (f.size() < 22)
        return false;

    // PF_KTHREAD is the authoritative kernel-thread flag (same as htop)
    static constexpr quint32 PF_KTHREAD = 0x00200000;
    const quint32 procFlags = f[6].toUInt();
    out.isKernelThread = (procFlags & PF_KTHREAD) != 0;

    out.state          = f[0].isEmpty() ? '?' : f[0].at(0);
    out.ppid           = f[1].toLong();
    out.cpuTicks       = f[11].toULongLong() + f[12].toULongLong(); // utime + stime
    out.priority       = f[15].toInt();
    out.nice           = f[16].toInt();
    out.threads        = f[17].toInt();
    out.startTimeTicks = f[19].toULongLong();
    out.vmSizeKb       = f[20].toULongLong() / 1024ULL;

    const long pageSize = sysconf(_SC_PAGESIZE);
    out.vmRssKb        = static_cast<quint64>(f[21].toLongLong())
                         * static_cast<quint64>(pageSize) / 1024ULL;

    // ── UID via stat() on /proc/pid directory ─────────────────────────────────
    // The /proc/<pid> directory is owned by the process's real UID — far more
    // reliable than parsing /proc/<pid>/status which may use spaces or tabs.
    {
        struct stat st{};
        if (::stat(QString("/proc/%1").arg(pid).toLocal8Bit().constData(), &st) == 0)
            out.uid = st.st_uid;
    }

    // Resolve uid → username (getpwuid is not thread-safe but fine here)
    const struct passwd *pw = getpwuid(out.uid);
    out.user = pw ? QString::fromUtf8(pw->pw_name) : QString::number(out.uid);

    // ── /proc/pid/cmdline ────────────────────────────────────────────────────
    // Kernel threads have no cmdline; use bracketed name as display string.
    if (out.isKernelThread)
    {
        out.cmdline = "[" + out.name + "]";
    }
    else
    {
        QFile cmdFile(QString("/proc/%1/cmdline").arg(pid));
        if (cmdFile.open(QIODevice::ReadOnly))
        {
            QByteArray data = cmdFile.readAll();
            cmdFile.close();
            data.replace('\0', ' ');
            out.cmdline = QString::fromUtf8(data).trimmed();
        }
        if (out.cmdline.isEmpty())
            out.cmdline = out.name; // fallback: use comm name
    }
    return true;
}

// ── Public: load all processes ────────────────────────────────────────────────

QList<Process> Process::loadAll()
{
    QList<Process> list;

    const QDir procDir("/proc");
    const QStringList entries =
        procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    list.reserve(entries.size());
    for (const QString &entry : entries)
    {
        bool ok;
        const pid_t pid = static_cast<pid_t>(entry.toInt(&ok));
        if (!ok)
            continue;

        Process proc;
        if (loadOne(pid, proc))
            list.append(proc);
    }

    return list;
}

} // namespace Os
