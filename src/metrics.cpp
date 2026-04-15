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

#include "metrics.h"
#include "configuration.h"

#include <QObject>
#include <QTimer>

CPU     Metrics::g_CPU;
GPU     Metrics::g_GPU;
Memory  Metrics::g_Memory;
Swap    Metrics::g_Swap;
Network Metrics::g_Network;
Storage Metrics::g_Storage;
Kernel  Metrics::g_Kernel;

Metrics *Metrics::Get()
{
    static Metrics instance(nullptr);
    return &instance;
}

Metrics::Metrics(QObject *parent) : QObject(parent)
{
    this->m_timer = new QTimer(this);
    connect(this->m_timer, &QTimer::timeout, this, &Metrics::onTimer);
    this->m_intervalMs = qMax(100, CFG->RefreshRateMs);
    this->m_timer->setInterval(this->m_intervalMs);

    // Prime baselines — first real sample will have valid deltas.
    this->sampleNow();

    this->m_timer->start(this->m_intervalMs);
}

void Metrics::SetInterval(int ms)
{
    this->m_intervalMs = qMax(100, ms);
    this->m_timer->setInterval(this->m_intervalMs);
}

void Metrics::SetActive(bool active)
{
    if (this->m_active == active)
        return;

    this->m_active = active;
    if (active)
    {
        // Refresh once immediately when entering Performance tab.
        this->sample();
        this->m_timer->start(this->m_intervalMs);
    } else
    {
        this->m_timer->stop();
    }
}

void Metrics::onTimer()
{
    if (!this->m_active)
        return;
    if (CFG->RefreshPaused)
        return;

    this->sample();
}

void Metrics::sample()
{
    if (CFG->RefreshPaused)
        return;

    const bool swapDevicesChanged = this->sampleNow();

    if (swapDevicesChanged)
        emit this->swapDevicesChanged();

    emit this->updated();
}

bool Metrics::sampleNow()
{
    bool swapDevicesChanged = false;

    if (this->m_cpuSamplingEnabled)
        Metrics::g_CPU.Sample();
    if (this->m_memorySamplingEnabled)
        Metrics::g_Memory.Sample();
    if (this->m_swapSamplingEnabled)
        Metrics::g_Swap.Sample(swapDevicesChanged);
    if (this->m_diskSamplingEnabled)
        Metrics::g_Storage.Sample();
    if (this->m_networkSamplingEnabled)
        Metrics::g_Network.Sample();
    if (this->m_gpuSamplingEnabled)
        Metrics::g_GPU.Sample();
    if (this->m_processStatsEnabled)
        Metrics::g_Kernel.Sample();

    return swapDevicesChanged;
}
