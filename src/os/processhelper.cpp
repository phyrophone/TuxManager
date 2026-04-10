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

#include "processhelper.h"

#include <cerrno>
#include <cstring>

#include <signal.h>
#include <sys/resource.h>

using namespace OS;

bool ProcessHelper::SendSignal(pid_t pid, int signal, QString &errorMsg)
{
    if (::kill(pid, signal) == 0)
        return true;
    errorMsg = QString("kill(%1, %2) failed: %3").arg(pid).arg(signal).arg(strerror(errno));
    return false;
}

bool ProcessHelper::Renice(pid_t pid, int nice, QString &errorMsg)
{
    errno = 0;
    if (setpriority(PRIO_PROCESS, static_cast<id_t>(pid), nice) == 0)
        return true;
    errorMsg = QString("setpriority(%1, %2) failed: %3").arg(pid).arg(nice).arg(strerror(errno));
    return false;
}

QString ProcessHelper::GetSignalName(int signal)
{
    switch (signal)
    {
        case SIGHUP:    return "SIGHUP (1)";
        case SIGINT:    return "SIGINT (2)";
        case SIGQUIT:   return "SIGQUIT (3)";
        case SIGILL:    return "SIGILL (4)";
        case SIGTRAP:   return "SIGTRAP (5)";
        case SIGABRT:   return "SIGABRT (6)";
        case SIGBUS:    return "SIGBUS (7)";
        case SIGFPE:    return "SIGFPE (8)";
        case SIGKILL:   return "SIGKILL (9)";
        case SIGUSR1:   return "SIGUSR1 (10)";
        case SIGSEGV:   return "SIGSEGV (11)";
        case SIGUSR2:   return "SIGUSR2 (12)";
        case SIGPIPE:   return "SIGPIPE (13)";
        case SIGALRM:   return "SIGALRM (14)";
        case SIGTERM:   return "SIGTERM (15)";
#ifdef SIGSTKFLT
        case SIGSTKFLT: return "SIGSTKFLT (16)";
#endif
        case SIGCHLD:   return "SIGCHLD (17)";
        case SIGCONT:   return "SIGCONT (18)";
        case SIGSTOP:   return "SIGSTOP (19)";
        case SIGTSTP:   return "SIGTSTP (20)";
        case SIGTTIN:   return "SIGTTIN (21)";
        case SIGTTOU:   return "SIGTTOU (22)";
        case SIGURG:    return "SIGURG (23)";
        case SIGXCPU:   return "SIGXCPU (24)";
        case SIGXFSZ:   return "SIGXFSZ (25)";
        case SIGVTALRM: return "SIGVTALRM (26)";
        case SIGPROF:   return "SIGPROF (27)";
        case SIGWINCH:  return "SIGWINCH (28)";
        case SIGIO:     return "SIGIO (29)";
#ifdef SIGPWR
        case SIGPWR:    return "SIGPWR (30)";
#endif
        case SIGSYS:    return "SIGSYS (31)";
        default:
            break;
    }

#if (defined SIGRTMIN) and (defined SIGRTMAX)
    if (signal >= SIGRTMIN && signal <= SIGRTMAX)
    {
        if (signal == SIGRTMIN)
            return QString("SIGRTMIN (%1)").arg(signal);
        if (signal == SIGRTMAX)
            return QString("SIGRTMAX (%1)").arg(signal);

        const int minOffset = signal - SIGRTMIN;
        const int maxOffset = SIGRTMAX - signal;
        if (minOffset <= maxOffset)
            return QString("SIGRTMIN+%1 (%2)").arg(minOffset).arg(signal);
        return QString("SIGRTMAX-%1 (%2)").arg(maxOffset).arg(signal);
    }
#endif

    return QString("Signal %1").arg(signal);
}
