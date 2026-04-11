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

#ifndef SWAP_H
#define SWAP_H

#include "../globals.h"
#include "../historybuffer.h"

#include <QElapsedTimer>
#include <QStringList>
#include <QString>
#include <memory>
#include <vector>

class Swap
{
    public:
        struct SwapInfo
        {
            QString       Name;     ///< Source path as reported by /proc/swaps
            QString       Type;     ///< partition/file/unknown
            qint64        TotalKb { 0 };
            qint64        UsedKb { 0 };
            qint64        FreeKb { 0 };
            int           Priority { 0 };
            HistoryBuffer UsageHistory { TUX_MANAGER_HISTORY_SIZE };
        };

        Swap();

        /// Sample /proc/swaps (+ /proc/vmstat for aggregate swap activity).
        bool Sample(bool &devicesChanged);

        int SwapCount() const { return static_cast<int>(this->m_swaps.size()); }
        const SwapInfo &FromIndex(int i) const;

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
        std::vector<std::unique_ptr<SwapInfo>> m_swaps;
        SwapInfo          m_nullSwap;
        qint64            m_swapTotalKb { 0 };
        qint64            m_swapUsedKb { 0 };
        qint64            m_swapFreeKb { 0 };
        quint64           m_prevSwapInPages { 0 };
        quint64           m_prevSwapOutPages { 0 };
        double            m_swapInBps { 0.0 };
        double            m_swapOutBps { 0.0 };
        double            m_swapMaxActivityBps { 0.0 };
        HistoryBuffer     m_swapUsageHistory { TUX_MANAGER_HISTORY_SIZE };
        HistoryBuffer     m_swapInHistory { TUX_MANAGER_HISTORY_SIZE };
        HistoryBuffer     m_swapOutHistory { TUX_MANAGER_HISTORY_SIZE };
        QElapsedTimer     m_swapTimer;
        qint64            m_prevSwapSampleMs { 0 };
};

#endif // SWAP_H
