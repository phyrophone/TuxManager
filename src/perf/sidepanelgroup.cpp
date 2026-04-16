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

#include "sidepanelgroup.h"

#include <QObject>

namespace Perf
{
    QString SidePanelGroupId(SidePanelGroup group)
    {
        switch (group)
        {
            case SidePanelGroup::Cpu:     return QStringLiteral("cpu");
            case SidePanelGroup::Memory:  return QStringLiteral("memory");
            case SidePanelGroup::Swap:    return QStringLiteral("swap");
            case SidePanelGroup::Disks:   return QStringLiteral("disks");
            case SidePanelGroup::Network: return QStringLiteral("network");
            case SidePanelGroup::Gpu:     return QStringLiteral("gpu");
        }

        return {};
    }

    QString SidePanelGroupLabel(SidePanelGroup group)
    {
        switch (group)
        {
            case SidePanelGroup::Cpu:     return QObject::tr("CPU");
            case SidePanelGroup::Memory:  return QObject::tr("Memory");
            case SidePanelGroup::Swap:    return QObject::tr("Swap");
            case SidePanelGroup::Disks:   return QObject::tr("Disks");
            case SidePanelGroup::Network: return QObject::tr("NICs");
            case SidePanelGroup::Gpu:     return QObject::tr("GPUs");
        }

        return {};
    }

    std::optional<SidePanelGroup> SidePanelGroupFromId(const QString &id)
    {
        if (id == QStringLiteral("cpu"))
            return SidePanelGroup::Cpu;
        if (id == QStringLiteral("memory"))
            return SidePanelGroup::Memory;
        if (id == QStringLiteral("swap"))
            return SidePanelGroup::Swap;
        if (id == QStringLiteral("disks"))
            return SidePanelGroup::Disks;
        if (id == QStringLiteral("network"))
            return SidePanelGroup::Network;
        if (id == QStringLiteral("gpu"))
            return SidePanelGroup::Gpu;
        return std::nullopt;
    }

    QList<SidePanelGroup> DefaultSidePanelGroupOrder()
    {
        return {
            SidePanelGroup::Cpu,
            SidePanelGroup::Memory,
            SidePanelGroup::Swap,
            SidePanelGroup::Disks,
            SidePanelGroup::Network,
            SidePanelGroup::Gpu
        };
    }
}
