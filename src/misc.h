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

#ifndef MISC_H
#define MISC_H

#include <QString>
#include <QtGlobal>

namespace Misc
{
    //! Takes time (seconds) and either returns a string (like "15 seconds") or if divisible by 60 or 3600
    //! return minutes / hours instead (so 600 returns "10 minutes"), used by UI
    QString SimplifyTime(int secs);

    //! Takes time in milliseconds and returns either "<n> ms" or "<n> s".
    QString SimplifyTimeMS(int ms);

    //! Formats bytes per second with binary units and B/s fallback for sub-KB values.
    QString FormatBytesPerSecond(double bytesPerSec);

    //! Formats a byte quantity using binary units (B, KB, MB, GB, TB).
    QString FormatBytes(quint64 bytes, int precision = 1);

    //! Formats a KiB quantity using binary units (B, KB, MB, GB, TB).
    QString FormatKiB(quint64 kibibytes, int precision = 1);

    //! Formats a MiB quantity using binary units (B, KB, MB, GB, TB).
    QString FormatMiB(quint64 mebibytes, int precision = 1);
}

#endif // MISC_H
