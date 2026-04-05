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

#include "configuration.h"
#include "colorscheme.h"

#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QVariantList>

Configuration *Configuration::s_instance = nullptr;

Configuration *Configuration::instance()
{
    if (!s_instance)
        s_instance = new Configuration();
    return s_instance;
}

Configuration::Configuration(QObject *parent) : QObject(parent)
{}

void Configuration::Load()
{
    QSettings s;

    // Window
    this->WindowGeometry = s.value("Window/Geometry").toByteArray();
    this->WindowState    = s.value("Window/State").toByteArray();
    this->ActiveTab      = s.value("Window/ActiveTab", this->ActiveTab).toInt();

    // General
    this->RefreshRateMs  = s.value("General/RefreshRateMs", this->RefreshRateMs).toInt();
    this->UseCustomColorScheme = s.value("General/UseCustomColorScheme",
                                         this->UseCustomColorScheme).toBool();
    this->CustomColorScheme = s.value("General/CustomColorScheme",
                                      this->CustomColorScheme).toMap();

    // Processes
    this->ShowKernelTasks        = s.value("Processes/ShowKernelTasks",        this->ShowKernelTasks).toBool();
    this->ShowOtherUsersProcs    = s.value("Processes/ShowOtherUsersProcs",    this->ShowOtherUsersProcs).toBool();
    this->ProcessListSortColumn  = s.value("Processes/SortColumn",             this->ProcessListSortColumn).toInt();
    this->ProcessListSortOrder   = s.value("Processes/SortOrder",              this->ProcessListSortOrder).toInt();

    // Performance / GPU selectors
    const QVariantList gpuSel = s.value("Performance/GpuEngineSelectorIndices").toList();
    if (!gpuSel.isEmpty())
    {
        this->GpuEngineSelectorIndices.clear();
        this->GpuEngineSelectorIndices.reserve(gpuSel.size());
        for (const QVariant &v : gpuSel)
            this->GpuEngineSelectorIndices.append(v.toInt());
    }

    while (this->GpuEngineSelectorIndices.size() < 4)
        this->GpuEngineSelectorIndices.append(this->GpuEngineSelectorIndices.size());
    if (this->GpuEngineSelectorIndices.size() > 4)
        this->GpuEngineSelectorIndices.resize(4);
    this->CpuGraphMode = s.value("Performance/CpuGraphMode", this->CpuGraphMode).toInt();
    if (this->CpuGraphMode != 0 && this->CpuGraphMode != 1)
        this->CpuGraphMode = 0;
    this->CpuShowKernelTimes = s.value("Performance/CpuShowKernelTimes",
                                       this->CpuShowKernelTimes).toBool();
    this->PerfShowCpu = s.value("Performance/ShowCpu", this->PerfShowCpu).toBool();
    this->PerfShowMemory = s.value("Performance/ShowMemory", this->PerfShowMemory).toBool();
    this->PerfShowSwap = s.value("Performance/ShowSwap", this->PerfShowSwap).toBool();
    this->PerfShowDisks = s.value("Performance/ShowDisks", this->PerfShowDisks).toBool();
    this->PerfShowNetwork = s.value("Performance/ShowNetwork", this->PerfShowNetwork).toBool();
    this->PerfShowGpu = s.value("Performance/ShowGpu", this->PerfShowGpu).toBool();
    this->PerfGraphWindowSec = s.value("Performance/GraphWindowSec", this->PerfGraphWindowSec).toInt();
    if (this->PerfGraphWindowSec != 60
        && this->PerfGraphWindowSec != 120
        && this->PerfGraphWindowSec != 300
        && this->PerfGraphWindowSec != 900)
    {
        this->PerfGraphWindowSec = 60;
    }

    // Color scheme
    ColorScheme *scheme = new ColorScheme(ColorScheme::DetectDarkMode()
                                          ? ColorScheme::DefaultDark()
                                          : ColorScheme::DefaultLight());
    if (this->UseCustomColorScheme)
        scheme->ApplyVariantMap(this->CustomColorScheme);
    ColorScheme::Install(scheme);
}

void Configuration::Save()
{
    QSettings s;

    // Window
    s.setValue("Window/Geometry",  this->WindowGeometry);
    s.setValue("Window/State",     this->WindowState);
    s.setValue("Window/ActiveTab", this->ActiveTab);

    // General
    s.setValue("General/RefreshRateMs", this->RefreshRateMs);
    s.setValue("General/UseCustomColorScheme", this->UseCustomColorScheme);
    s.setValue("General/CustomColorScheme",
               this->UseCustomColorScheme
               ? ColorScheme::GetCurrent()->ToVariantMap()
               : this->CustomColorScheme);

    // Processes
    s.setValue("Processes/ShowKernelTasks",     this->ShowKernelTasks);
    s.setValue("Processes/ShowOtherUsersProcs", this->ShowOtherUsersProcs);
    s.setValue("Processes/SortColumn",          this->ProcessListSortColumn);
    s.setValue("Processes/SortOrder",           this->ProcessListSortOrder);

    QVariantList gpuSel;
    gpuSel.reserve(this->GpuEngineSelectorIndices.size());
    for (int v : this->GpuEngineSelectorIndices)
        gpuSel.append(v);
    s.setValue("Performance/GpuEngineSelectorIndices", gpuSel);
    s.setValue("Performance/CpuGraphMode", this->CpuGraphMode);
    s.setValue("Performance/CpuShowKernelTimes", this->CpuShowKernelTimes);
    s.setValue("Performance/ShowCpu", this->PerfShowCpu);
    s.setValue("Performance/ShowMemory", this->PerfShowMemory);
    s.setValue("Performance/ShowSwap", this->PerfShowSwap);
    s.setValue("Performance/ShowDisks", this->PerfShowDisks);
    s.setValue("Performance/ShowNetwork", this->PerfShowNetwork);
    s.setValue("Performance/ShowGpu", this->PerfShowGpu);
    s.setValue("Performance/GraphWindowSec", this->PerfGraphWindowSec);

    s.sync();
}
