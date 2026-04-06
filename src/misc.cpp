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

#include "misc.h"

#include <array>
#include <QObject>
#include <QtGlobal>

QString Misc::SimplifyTime(int secs)
{
    if (secs < 0)
        secs = 0;

    if (secs > 0 && (secs % 3600) == 0)
    {
        const int hours = secs / 3600;
        return QString::number(hours) + ((hours == 1) ? " hour" : " hours");
    }

    if (secs > 0 && (secs % 60) == 0)
    {
        const int minutes = secs / 60;
        return QString::number(minutes) + ((minutes == 1) ? " minute" : " minutes");
    }

    return QString::number(secs) + ((secs == 1) ? " second" : " seconds");
}

QString Misc::FormatBytesPerSecond(double bytesPerSec)
{
    const double v = (bytesPerSec < 0.0) ? 0.0 : bytesPerSec;

    if (v >= 1024.0 * 1024.0)
        return QString::number(v / (1024.0 * 1024.0), 'f', 1) + QObject::tr(" MB/s");

    if (v >= 1024.0)
        return QString::number(v / 1024.0, 'f', 0) + QObject::tr(" KB/s");

    return QString::number(v, 'f', 0) + QObject::tr(" B/s");
}

QString Misc::FormatBytes(quint64 bytes, int precision)
{
    static const std::array<const char *, 5> kUnits = { " B", " KB", " MB", " GB", " TB" };

    double value = static_cast<double>(bytes);
    int unitIndex = 0;
    while (value >= 1024.0 && unitIndex < static_cast<int>(kUnits.size()) - 1)
    {
        value /= 1024.0;
        ++unitIndex;
    }

    const int decimals = (unitIndex == 0) ? 0 : qMax(0, precision);
    return QString::number(value, 'f', decimals) + QObject::tr(kUnits[unitIndex]);
}

QString Misc::FormatKiB(quint64 kibibytes, int precision)
{
    return FormatBytes(kibibytes * 1024ULL, precision);
}

QString Misc::FormatMiB(quint64 mebibytes, int precision)
{
    return FormatBytes(mebibytes * 1024ULL * 1024ULL, precision);
}
