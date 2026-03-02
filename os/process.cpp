#include "process.h"

#include <QDir>
#include <QFile>

#include <pwd.h>
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

    // Fields after ") ": state ppid pgrp session tty_nr tpgid flags
    //   minflt cminflt majflt cmajflt utime(13) stime(14) cutime cstime
    //   priority(17) nice(18) num_threads(19) itrealvalue starttime(21) vsize(22) rss(23) ...
    // (0-indexed from the first field after the closing paren)
    const QList<QByteArray> f = statData.mid(commEnd + 2).split(' ');
    if (f.size() < 24)
        return false;

    out.state          = f[0].isEmpty() ? '?' : f[0].at(0);
    out.ppid           = f[1].toLong();
    out.cpuTicks       = f[13].toULongLong() + f[14].toULongLong(); // utime + stime
    out.priority       = f[17].toInt();
    out.nice           = f[18].toInt();
    out.threads        = f[19].toInt();
    out.startTimeTicks = f[21].toULongLong();
    out.vmSizeKb       = f[22].toULongLong() / 1024ULL;

    const long pageSize = sysconf(_SC_PAGESIZE);
    out.vmRssKb        = static_cast<quint64>(f[23].toLongLong())
                         * static_cast<quint64>(pageSize) / 1024ULL;

    // ── /proc/pid/status (uid) ───────────────────────────────────────────────
    QFile statusFile(QString("/proc/%1/status").arg(pid));
    if (statusFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        while (!statusFile.atEnd())
        {
            const QByteArray line = statusFile.readLine();
            if (line.startsWith("Uid:"))
            {
                // Uid: real  effective  saved  filesystem
                const QList<QByteArray> parts =
                    line.mid(4).trimmed().split('\t');
                if (!parts.isEmpty())
                    out.uid = parts[0].trimmed().toUInt();
                break;
            }
        }
        statusFile.close();
    }

    // Resolve uid → username (getpwuid is not thread-safe but fine here)
    const struct passwd *pw = getpwuid(out.uid);
    out.user = pw ? QString::fromUtf8(pw->pw_name) : QString::number(out.uid);

    // ── /proc/pid/cmdline ────────────────────────────────────────────────────
    QFile cmdFile(QString("/proc/%1/cmdline").arg(pid));
    if (cmdFile.open(QIODevice::ReadOnly))
    {
        QByteArray data = cmdFile.readAll();
        cmdFile.close();
        data.replace('\0', ' ');
        out.cmdline = QString::fromUtf8(data).trimmed();
    }
    if (out.cmdline.isEmpty())
    {
        out.isKernelThread = true;
        out.cmdline = "[" + out.name + "]";
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
