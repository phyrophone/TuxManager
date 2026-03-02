#ifndef OS_PROCESSHELPER_H
#define OS_PROCESSHELPER_H

#include <QString>
#include <sys/types.h>

namespace Os
{

class ProcessHelper
{
public:
    /// Send an arbitrary signal number to a process.
    /// Returns true on success, sets errorMsg on failure.
    static bool sendSignal(pid_t pid, int signal, QString &errorMsg);

    /// Convenience wrappers for common signals.
    static bool kill(pid_t pid, QString &errorMsg);   ///< SIGKILL (9)
    static bool term(pid_t pid, QString &errorMsg);   ///< SIGTERM (15)
    static bool hup(pid_t pid, QString &errorMsg);    ///< SIGHUP  (1)
    static bool stop(pid_t pid, QString &errorMsg);   ///< SIGSTOP (19)
    static bool cont(pid_t pid, QString &errorMsg);   ///< SIGCONT (18)

    /// Change the nice value of a process.
    /// Returns true on success, sets errorMsg on failure.
    static bool renice(pid_t pid, int nice, QString &errorMsg);

    /// Human-readable description of a signal number.
    static QString signalName(int signal);

private:
    ProcessHelper() = delete;
};

} // namespace Os

#endif // OS_PROCESSHELPER_H
