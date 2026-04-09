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

#ifndef STORAGE_H
#define STORAGE_H

#include "../globals.h"
#include "../historybuffer.h"
#include <QElapsedTimer>
#include <QSet>

class Storage
{
    public:
        Storage();

        /// Sample /proc/diskstats and compute per-device active time % and read/write throughput.
        bool Sample();

        int DiskCount() const { return this->m_disks.size(); }
        QString DiskName(int i) const;
        QString DiskModel(int i) const;
        QString DiskType(int i) const;
        double DiskActivePercent(int i) const;
        double DiskReadBytesPerSec(int i) const;
        double DiskWriteBytesPerSec(int i) const;
        double DiskMaxTransferBytesPerSec(int i) const;
        qint64 DiskCapacityBytes(int i) const;
        qint64 DiskFormattedBytes(int i) const;
        bool DiskIsSystemDisk(int i) const;
        bool DiskHasSwapFile(int i) const;
        const HistoryBuffer &DiskActiveHistory(int i) const;
        const HistoryBuffer &DiskReadHistory(int i) const;
        const HistoryBuffer &DiskWriteHistory(int i) const;


    private:
        struct DiskSample
        {
            QString        Name;          ///< base device name (e.g. sda, nvme0n1)
            QString        Model;         ///< best-effort model string
            QString        Type;          ///< HDD/SSD/Unknown
            quint64        PrevReadSecs  { 0 };
            quint64        PrevWriteSecs { 0 };
            quint64        PrevIoMs      { 0 };
            double         ActivePct     { 0.0 };
            double         ReadBps       { 0.0 };
            double         WriteBps      { 0.0 };
            double         MaxTransferBps { 0.0 };
            qint64         CapacityBytes { 0 };
            qint64         FormattedBytes { 0 };
            bool           IsSystemDisk { false };
            bool           HasPageFile { false };
            HistoryBuffer ActiveHistory { TUX_MANAGER_HISTORY_SIZE };
            HistoryBuffer ReadHistory { TUX_MANAGER_HISTORY_SIZE };
            HistoryBuffer WriteHistory { TUX_MANAGER_HISTORY_SIZE };
        };

        static QStringList listTrackedBlockDevices(const QSet<QString> &measurableDevices);
        static QSet<QString> resolveBaseBlockDevices(const QString &devName);
        static bool shouldIgnoreBlockDevice(const QString &baseName);

        void refreshDisks(const QSet<QString> &measurableDevices);

        QVector<DiskSample> m_disks;
        QElapsedTimer       m_diskTimer;
        qint64              m_prevDiskSampleMs { 0 };
};

#endif // STORAGE_H
