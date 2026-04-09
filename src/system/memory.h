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

#ifndef MEMORY_H
#define MEMORY_H

#include "../globals.h"
#include "../historybuffer.h"
#include <QElapsedTimer>
#include <QString>

class Memory
{
    public:
        Memory();

        /// Sample /proc/meminfo (+ /proc/vmstat for swap activity) and append memory/swap histories.
        bool Sample();

        qint64 MemTotalKb()   const { return this->m_memTotalKb;   }
        /// In-use (htop formula): Total - Free - Buffers - PageCache
        qint64 MemUsedKb()    const { return this->m_memUsedKb;    }
        qint64 MemAvailKb()   const { return this->m_memAvailKb;   }
        /// Truly free (MemFree from /proc/meminfo)
        qint64 MemFreeKb()    const { return this->m_memFreeKb;    }
        /// Page cache: Cached + SReclaimable - Shmem + Buffers
        qint64 MemCachedKb()  const { return this->m_memCachedKb;  }
        qint64 MemBuffersKb() const { return this->m_memBuffersKb; }
        /// Dirty pages pending write-back: Dirty + Writeback
        qint64 MemDirtyKb()   const { return this->m_memDirtyKb;   }
        int MemDimmSlotsTotal() const { return this->m_memDimmSlotsTotal; }
        int MemDimmSlotsUsed()  const { return this->m_memDimmSlotsUsed;  }
        int MemSpeedMtps()      const { return this->m_memSpeedMtps;      }
        double MemFraction()  const;
        const HistoryBuffer &MemHistory() const { return this->m_memHistory; }
        void SetSwapSamplingEnabled(bool enabled) { this->m_swapSamplingEnabled = enabled; }

        qint64 SwapTotalKb() const { return this->m_swapTotalKb; }
        qint64 SwapUsedKb()  const { return this->m_swapUsedKb;  }
        qint64 SwapFreeKb()  const { return this->m_swapFreeKb;  }
        double SwapInBytesPerSec()  const { return this->m_swapInBps;  }
        double SwapOutBytesPerSec() const { return this->m_swapOutBps; }
        double SwapMaxActivityBytesPerSec() const { return this->m_swapMaxActivityBps; }
        const HistoryBuffer &SwapUsageHistory() const { return this->m_swapUsageHistory; }
        const HistoryBuffer &SwapInHistory()    const { return this->m_swapInHistory;    }
        const HistoryBuffer &SwapOutHistory()   const { return this->m_swapOutHistory;   }

    private:
        void readHardwareMetadata();

        bool             m_swapSamplingEnabled { true };
        qint64           m_memTotalKb   { 0 };
        qint64           m_memUsedKb    { 0 };
        qint64           m_memAvailKb   { 0 };
        qint64           m_memFreeKb    { 0 };
        qint64           m_memCachedKb  { 0 };
        qint64           m_memBuffersKb { 0 };
        qint64           m_memDirtyKb   { 0 };
        int              m_memDimmSlotsTotal { 0 };
        int              m_memDimmSlotsUsed  { 0 };
        int              m_memSpeedMtps      { 0 };
        HistoryBuffer    m_memHistory { TUX_MANAGER_HISTORY_SIZE };

        qint64           m_swapTotalKb   { 0 };
        qint64           m_swapUsedKb    { 0 };
        qint64           m_swapFreeKb    { 0 };
        quint64          m_prevSwapInPages  { 0 };
        quint64          m_prevSwapOutPages { 0 };
        double           m_swapInBps  { 0.0 };
        double           m_swapOutBps { 0.0 };
        double           m_swapMaxActivityBps { 0.0 };
        HistoryBuffer    m_swapUsageHistory { TUX_MANAGER_HISTORY_SIZE };
        HistoryBuffer    m_swapInHistory { TUX_MANAGER_HISTORY_SIZE };
        HistoryBuffer    m_swapOutHistory { TUX_MANAGER_HISTORY_SIZE };
        QElapsedTimer    m_swapTimer;
        qint64           m_prevSwapSampleMs { 0 };
};

#endif // MEMORY_H
