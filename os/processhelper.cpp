#include "processhelper.h"

#include <cerrno>
#include <cstring>

#include <signal.h>
#include <sys/resource.h>

namespace Os
{

// ── Signal sending ────────────────────────────────────────────────────────────

bool ProcessHelper::sendSignal(pid_t pid, int signal, QString &errorMsg)
{
    if (::kill(pid, signal) == 0)
        return true;
    errorMsg = QString("kill(%1, %2) failed: %3")
               .arg(pid).arg(signal).arg(strerror(errno));
    return false;
}

bool ProcessHelper::kill(pid_t pid, QString &errorMsg)
{
    return sendSignal(pid, SIGKILL, errorMsg);
}

bool ProcessHelper::term(pid_t pid, QString &errorMsg)
{
    return sendSignal(pid, SIGTERM, errorMsg);
}

bool ProcessHelper::hup(pid_t pid, QString &errorMsg)
{
    return sendSignal(pid, SIGHUP, errorMsg);
}

bool ProcessHelper::stop(pid_t pid, QString &errorMsg)
{
    return sendSignal(pid, SIGSTOP, errorMsg);
}

bool ProcessHelper::cont(pid_t pid, QString &errorMsg)
{
    return sendSignal(pid, SIGCONT, errorMsg);
}

// ── Priority ──────────────────────────────────────────────────────────────────

bool ProcessHelper::renice(pid_t pid, int nice, QString &errorMsg)
{
    errno = 0;
    if (setpriority(PRIO_PROCESS, static_cast<id_t>(pid), nice) == 0)
        return true;
    errorMsg = QString("setpriority(%1, %2) failed: %3")
               .arg(pid).arg(nice).arg(strerror(errno));
    return false;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QString ProcessHelper::signalName(int signal)
{
    switch (signal)
    {
        case SIGHUP:    return "SIGHUP (1)";
        case SIGINT:    return "SIGINT (2)";
        case SIGQUIT:   return "SIGQUIT (3)";
        case SIGKILL:   return "SIGKILL (9)";
        case SIGUSR1:   return "SIGUSR1 (10)";
        case SIGUSR2:   return "SIGUSR2 (12)";
        case SIGTERM:   return "SIGTERM (15)";
        case SIGCONT:   return "SIGCONT (18)";
        case SIGSTOP:   return "SIGSTOP (19)";
        case SIGTSTP:   return "SIGTSTP (20)";
        default:        return QString("Signal %1").arg(signal);
    }
}

} // namespace Os
