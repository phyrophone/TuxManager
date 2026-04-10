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

#ifndef GPU_H
#define GPU_H

#include "../globals.h"
#include "../historybuffer.h"
#include <QSet>
#include <QElapsedTimer>

class GPU
{
    public:
        struct GPUEngineInfo
        {
            QString         Key;
            QString         Label;
            double          Pct { 0.0 };
            HistoryBuffer   History { TUX_MANAGER_HISTORY_SIZE };
        };

        struct GPUInfo
        {
            QString         ID;
            QString         Name;
            QString         DriverVersion;
            QString         Backend;
            double          UtilPct { 0.0 };
            int             TemperatureC { -1 };
            qint64          MemUsedMiB { 0 };
            qint64          MemTotalMiB { 0 };
            qint64          SharedMemUsedMiB { 0 };
            qint64          SharedMemTotalMiB { 0 };
            double          CopyTxBps { 0.0 };
            double          CopyRxBps { 0.0 };
            double          MaxCopyBps { 0.0 };
            HistoryBuffer   UtilHistory { TUX_MANAGER_HISTORY_SIZE };
            HistoryBuffer   MemUsageHistory { TUX_MANAGER_HISTORY_SIZE };
            HistoryBuffer   SharedMemHistory { TUX_MANAGER_HISTORY_SIZE };
            HistoryBuffer   CopyTxHistory { TUX_MANAGER_HISTORY_SIZE };
            HistoryBuffer   CopyRxHistory { TUX_MANAGER_HISTORY_SIZE };
            QVector<GPUEngineInfo>    Engines;
            QHash<QString, qint64>      PrevFDInfoEngineNs;
        };

        // Cached sysfs paths for a kernel DRM-managed GPU.
        // Populated once at startup; used every sampling tick.
        struct DRMCard
        {
            QString   ID;             // PCI address, e.g. 0000:05:00.0
            QString   Vendor;         // PCI vendor, e.g. "0x1002"
            QString   DriverName;     // e.g. "amdgpu", "i915"
            QString   DriverVersion;
            QString   BusyPath;       // .../gpu_busy_percent
            QString   VramTotalPath;  // .../mem_info_vram_total
            QString   VramUsedPath;   // .../mem_info_vram_used
            QString   GttTotalPath;   // .../mem_info_gtt_total  (shared / system)
            QString   GttUsedPath;    // .../mem_info_gtt_used
            QString   TempPath;       // hwmon temp1_input (milli-°C)
            QString   RenderNodePath; // /dev/dri/renderDN  (for fdinfo matching)
            QString   CardNodePath;   // /dev/dri/cardN

            // All *_busy_percent engine files (excluding gpu_busy_percent).
            QVector<QPair<QString, QString>>  EngineBusyPaths; // (key, sysfs path)

            // Cached fdinfo paths from the last full /proc scan.
            QStringList     CachedFDInfoPaths;
        };

        GPU();
        ~GPU();

        /// Sample GPU backends (NVML when available) and update utilization/memory/engine histories.
        bool Sample();

        int GpuCount() const { return this->m_gpus.size(); }
        const GPUInfo &FromIndex(int gpuIndex) const;

    private:
        void detectGpuBackends();
        void detectDrmCards();
        bool sampleNvml();
        bool sampleDrm();
        QHash<QString, qint64> scanDrmFdInfoEngines(DRMCard &card);
        void unloadGpuBackends();
        //! Returned if index of requested GPU doesn't exist
        GPUInfo m_nullGPU;

        QVector<GPUInfo>    m_gpus;
        bool                m_hasNvml { false };
        void               *m_nvmlLibHandle { nullptr };
        QVector<DRMCard>    m_drmCards;
        QElapsedTimer       m_gpuFdInfoTimer;
        bool                m_gpuFdInfoTimerStarted { false };
        int                 m_gpuFdInfoRescanCounter { 0 };
};

#endif // GPU_H
