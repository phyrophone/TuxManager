#ifndef OS_PROCESS_H
#define OS_PROCESS_H

#include <QList>
#include <QString>
#include <sys/types.h>

namespace Os
{

/// Snapshot of a single Linux process read from /proc.
struct Process
{
    pid_t   pid           { 0 };
    pid_t   ppid          { 0 };
    QString name;                     ///< Short name  (/proc/pid/comm)
    QString cmdline;                  ///< Full command (/proc/pid/cmdline)
    char    state         { '?' };    ///< Raw state char: R S D Z T I ...
    uid_t   uid           { 0 };
    QString user;                     ///< Resolved username
    int     priority      { 0 };
    int     nice          { 0 };
    int     threads       { 1 };
    quint64 vmRssKb       { 0 };      ///< Resident set size in KiB
    quint64 vmSizeKb      { 0 };      ///< Virtual memory size in KiB
    quint64 cpuTicks      { 0 };      ///< utime + stime in jiffies (for delta CPU%)
    double  cpuPercent    { 0.0 };    ///< Calculated externally after two samples
    quint64 startTimeTicks{ 0 };      ///< Start time in jiffies since boot
    bool    isKernelThread{ false };   ///< True when /proc/pid/cmdline is empty (kernel thread)

    /// Load a snapshot of every running process from /proc.
    static QList<Process> loadAll();

    /// Human-readable description of a raw state character.
    static QString stateString(char state);

private:
    static bool loadOne(pid_t pid, Process &out);
};

} // namespace Os

#endif // OS_PROCESS_H
