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

#ifndef GPUINTELSYSMANBACKEND_H
#define GPUINTELSYSMANBACKEND_H

#include "../gpu.h"

#include <QHash>
#include <memory>
#include <vector>

class GpuIntelSysmanBackend
{
    public:
        GpuIntelSysmanBackend() = default;
        ~GpuIntelSysmanBackend();

        void Detect();
        bool Sample(std::vector<std::unique_ptr<GPU::GPUInfo>> &gpus);
        bool IsAvailable() const { return this->m_available; }

    private:
        struct Snapshot
        {
            uint64_t value { 0 };
            uint64_t timestamp { 0 };
        };

        void unload();

        bool    m_available { false };
        void   *m_libHandle { nullptr };
        QHash<QString, Snapshot> m_prevEnergyById;
        QHash<QString, Snapshot> m_prevPciRxById;
        QHash<QString, Snapshot> m_prevPciTxById;
        QHash<QString, Snapshot> m_prevEngineByKey;
};

#endif // GPUINTELSYSMANBACKEND_H
