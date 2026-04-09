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

#ifndef METRICS_H
#define METRICS_H

#include "system/cpu.h"
#include "system/gpu.h"
#include "system/memory.h"
#include "system/network.h"
#include "system/storage.h"
#include "system/kernel.h"
#include <QObject>

class QTimer;

/// Periodically samples /proc/stat (CPU) and /proc/meminfo (memory).
/// All widgets that display performance data connect to the updated() signal
/// and read values through the const accessors.
class Metrics : public QObject
{
    Q_OBJECT

    public:
        static CPU      *GetCPU()       { return &Metrics::g_CPU; }
        static GPU      *GetGPU()       { return &Metrics::g_GPU; }
        static Network  *GetNetwork()   { return &Metrics::g_Network; }
        static Memory   *GetMemory()    { return &Metrics::g_Memory; }
        static Storage  *GetStorage()   { return &Metrics::g_Storage; }
        static Kernel   *GetKernel()    { return &Metrics::g_Kernel; }

        Metrics(QObject *parent);
        ~Metrics();

        void SetInterval(int ms);
        void SetActive(bool active);
        bool IsActive() const { return this->m_active; }
        void SetProcessStatsEnabled(bool enabled) { this->m_processStatsEnabled = enabled; }
        void SetCpuSamplingEnabled(bool enabled) { this->m_cpuSamplingEnabled = enabled; }
        void SetMemorySamplingEnabled(bool enabled) { this->m_memorySamplingEnabled = enabled; }
        void SetDiskSamplingEnabled(bool enabled) { this->m_diskSamplingEnabled = enabled; }
        void SetNetworkSamplingEnabled(bool enabled) { this->m_networkSamplingEnabled = enabled; }
        void SetGpuSamplingEnabled(bool enabled) { this->m_gpuSamplingEnabled = enabled; }

    signals:
        void updated();

    private slots:
        void onTimer();

    private:
        static CPU     g_CPU;
        static GPU     g_GPU;
        static Memory  g_Memory;
        static Network g_Network;
        static Storage g_Storage;
        static Kernel  g_Kernel;

        //! Run all samples that are enabled
        void sample();

        QTimer *m_timer;
        int     m_intervalMs { 1000 };
        bool    m_active { true };
        bool    m_processStatsEnabled { false };
        bool    m_cpuSamplingEnabled { true };
        bool    m_memorySamplingEnabled { true };
        bool    m_diskSamplingEnabled { true };
        bool    m_networkSamplingEnabled { true };
        bool    m_gpuSamplingEnabled { true };
};

#endif // METRICS_H
