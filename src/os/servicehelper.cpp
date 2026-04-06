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

#include "servicehelper.h"

#include <dlfcn.h>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

using namespace OS;

namespace
{
    struct sd_bus;
    struct sd_bus_message;
    struct sd_bus_error;

    using FnSdBusOpenSystem = int (*)(sd_bus **);
    using FnSdBusUnref = sd_bus *(*)(sd_bus *);
    using FnSdBusCallMethod = int (*)(sd_bus *, const char *, const char *, const char *,
                                      const char *, sd_bus_error *, sd_bus_message **,
                                      const char *, ...);
    using FnSdBusMessageUnref = sd_bus_message *(*)(sd_bus_message *);
    using FnSdBusMessageEnterContainer = int (*)(sd_bus_message *, char, const char *);
    using FnSdBusMessageExitContainer = int (*)(sd_bus_message *);
    using FnSdBusMessageRead = int (*)(sd_bus_message *, const char *, ...);

    void *g_sdLib = nullptr;
    FnSdBusOpenSystem pSdBusOpenSystem = nullptr;
    FnSdBusUnref pSdBusUnref = nullptr;
    FnSdBusCallMethod pSdBusCallMethod = nullptr;
    FnSdBusMessageUnref pSdBusMessageUnref = nullptr;
    FnSdBusMessageEnterContainer pSdBusMessageEnterContainer = nullptr;
    FnSdBusMessageExitContainer pSdBusMessageExitContainer = nullptr;
    FnSdBusMessageRead pSdBusMessageRead = nullptr;

    bool ensureSdBusLoaded(QString *error)
    {
        if (pSdBusOpenSystem && pSdBusCallMethod && pSdBusMessageRead)
            return true;

        if (!g_sdLib)
        {
            g_sdLib = ::dlopen("libsystemd.so.0", RTLD_LAZY | RTLD_LOCAL);
            if (!g_sdLib)
                g_sdLib = ::dlopen("libsystemd.so", RTLD_LAZY | RTLD_LOCAL);
        }
        if (!g_sdLib)
        {
            if (error)
                *error = QObject::tr("libsystemd not found");
            return false;
        }

        pSdBusOpenSystem = reinterpret_cast<FnSdBusOpenSystem>(::dlsym(g_sdLib, "sd_bus_open_system"));
        pSdBusUnref = reinterpret_cast<FnSdBusUnref>(::dlsym(g_sdLib, "sd_bus_unref"));
        pSdBusCallMethod = reinterpret_cast<FnSdBusCallMethod>(::dlsym(g_sdLib, "sd_bus_call_method"));
        pSdBusMessageUnref = reinterpret_cast<FnSdBusMessageUnref>(::dlsym(g_sdLib, "sd_bus_message_unref"));
        pSdBusMessageEnterContainer = reinterpret_cast<FnSdBusMessageEnterContainer>(::dlsym(g_sdLib, "sd_bus_message_enter_container"));
        pSdBusMessageExitContainer = reinterpret_cast<FnSdBusMessageExitContainer>(::dlsym(g_sdLib, "sd_bus_message_exit_container"));
        pSdBusMessageRead = reinterpret_cast<FnSdBusMessageRead>(::dlsym(g_sdLib, "sd_bus_message_read"));

        if (!pSdBusOpenSystem || !pSdBusUnref || !pSdBusCallMethod || !pSdBusMessageUnref
            || !pSdBusMessageEnterContainer || !pSdBusMessageExitContainer || !pSdBusMessageRead)
        {
            if (error)
                *error = QObject::tr("libsystemd missing sd-bus symbols");
            return false;
        }
        if (error)
            error->clear();
        return true;
    }
} // namespace

bool ServiceHelper::IsSystemdAvailable(QString *reason)
{
    const QString exe = QStandardPaths::findExecutable("systemctl");
    if (exe.isEmpty())
    {
        if (reason)
            *reason = QObject::tr("systemctl not found");
        return false;
    }

    if (!QFileInfo::exists("/run/systemd/system"))
    {
        if (reason)
            *reason = QObject::tr("systemd runtime directory not present");
        return false;
    }

    if (reason)
        reason->clear();
    return true;
}

bool ServiceHelper::RunSystemctl(const QStringList &args,
                                 QString          &stdoutText,
                                 QString          &stderrText,
                                 int              &exitCode,
                                 int               timeoutMs)
{
    stdoutText.clear();
    stderrText.clear();
    exitCode = -1;

    const QString exe = QStandardPaths::findExecutable("systemctl");
    if (exe.isEmpty())
    {
        stderrText = QObject::tr("systemctl not found");
        return false;
    }

    QProcess p;
    p.start(exe, args);
    if (!p.waitForStarted(500))
    {
        stderrText = QObject::tr("Failed to start systemctl");
        return false;
    }

    if (!p.waitForFinished(timeoutMs))
    {
        p.kill();
        p.waitForFinished(200);
        stderrText = QObject::tr("systemctl timed out");
        return false;
    }

    stdoutText = QString::fromUtf8(p.readAllStandardOutput());
    stderrText = QString::fromUtf8(p.readAllStandardError());
    exitCode = p.exitCode();
    return p.exitStatus() == QProcess::NormalExit;
}

bool ServiceHelper::ListServicesViaSystemdDbus(QList<ServiceRecord> &records, QString *error)
{
    records.clear();

    QString loadErr;
    if (!ensureSdBusLoaded(&loadErr))
    {
        if (error)
            *error = loadErr;
        return false;
    }

    sd_bus *bus = nullptr;
    sd_bus_message *reply = nullptr;

    int r = pSdBusOpenSystem(&bus);
    if (r < 0 || !bus)
    {
        if (error)
            *error = QObject::tr("sd-bus open system failed (%1)").arg(r);
        return false;
    }

    r = pSdBusCallMethod(bus,
                         "org.freedesktop.systemd1",
                         "/org/freedesktop/systemd1",
                         "org.freedesktop.systemd1.Manager",
                         "ListUnits",
                         nullptr,
                         &reply,
                         nullptr);
    if (r < 0 || !reply)
    {
        if (error)
            *error = QObject::tr("sd-bus ListUnits call failed (%1)").arg(r);
        if (reply)
            pSdBusMessageUnref(reply);
        pSdBusUnref(bus);
        return false;
    }

    r = pSdBusMessageEnterContainer(reply, 'a', "(ssssssouso)");
    if (r < 0)
    {
        if (error)
            *error = QObject::tr("sd-bus decode failed (%1)").arg(r);
        pSdBusMessageUnref(reply);
        pSdBusUnref(bus);
        return false;
    }

    while ((r = pSdBusMessageEnterContainer(reply, 'r', "ssssssouso")) > 0)
    {
        const char *unit = nullptr;
        const char *description = nullptr;
        const char *loadState = nullptr;
        const char *activeState = nullptr;
        const char *subState = nullptr;
        const char *following = nullptr;
        const char *objectPath = nullptr;
        unsigned int jobId = 0;
        const char *jobType = nullptr;
        const char *jobPath = nullptr;

        const int rr = pSdBusMessageRead(reply, "ssssssouso",
                                         &unit,
                                         &description,
                                         &loadState,
                                         &activeState,
                                         &subState,
                                         &following,
                                         &objectPath,
                                         &jobId,
                                         &jobType,
                                         &jobPath);
        const int er = pSdBusMessageExitContainer(reply); // row
        Q_UNUSED(following);
        Q_UNUSED(objectPath);
        Q_UNUSED(jobId);
        Q_UNUSED(jobType);
        Q_UNUSED(jobPath);

        if (rr < 0 || er < 0)
        {
            if (error)
                *error = QObject::tr("sd-bus read row failed (%1/%2)").arg(rr).arg(er);
            pSdBusMessageExitContainer(reply); // array (best effort)
            pSdBusMessageUnref(reply);
            pSdBusUnref(bus);
            return false;
        }

        const QString unitName = QString::fromLatin1(unit ? unit : "");
        if (!unitName.endsWith(".service"))
            continue;

        ServiceRecord rec;
        rec.unit = unitName;
        rec.description = QString::fromLatin1(description ? description : "");
        rec.loadState = QString::fromLatin1(loadState ? loadState : "");
        rec.activeState = QString::fromLatin1(activeState ? activeState : "");
        rec.subState = QString::fromLatin1(subState ? subState : "");
        records.append(rec);
    }

    pSdBusMessageExitContainer(reply); // array
    pSdBusMessageUnref(reply);
    pSdBusUnref(bus);

    if (r < 0)
    {
        if (error)
            *error = QObject::tr("sd-bus iteration failed (%1)").arg(r);
        return false;
    }

    if (error)
        error->clear();
    return true;
}

