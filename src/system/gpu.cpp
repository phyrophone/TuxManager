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

#include "gpu.h"
#include "../configuration.h"
#include "../misc.h"
#include "../logger.h"
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <dlfcn.h>
#include <unistd.h>

namespace
{
    using NvmlReturn = unsigned int;
    using NvmlDevice = void *;

    struct NvmlUtilization
    {
        unsigned int gpu;
        unsigned int memory;
    };

    struct NvmlMemory
    {
        uint64_t total;
        uint64_t free;
        uint64_t used;
    };

    static constexpr NvmlReturn NVML_SUCCESS = 0;

    using FnNvmlInitV2 = NvmlReturn (*)();
    using FnNvmlShutdown = NvmlReturn (*)();
    using FnNvmlSystemGetDriverVersion = NvmlReturn (*)(char *, unsigned int);
    using FnNvmlDeviceGetCountV2 = NvmlReturn (*)(unsigned int *);
    using FnNvmlDeviceGetHandleByIndexV2 = NvmlReturn (*)(unsigned int, NvmlDevice *);
    using FnNvmlDeviceGetName = NvmlReturn (*)(NvmlDevice, char *, unsigned int);
    using FnNvmlDeviceGetUUID = NvmlReturn (*)(NvmlDevice, char *, unsigned int);
    using FnNvmlDeviceGetUtilizationRates = NvmlReturn (*)(NvmlDevice, NvmlUtilization *);
    using FnNvmlDeviceGetMemoryInfo = NvmlReturn (*)(NvmlDevice, NvmlMemory *);
    using FnNvmlDeviceGetEncoderUtilization = NvmlReturn (*)(NvmlDevice, unsigned int *, unsigned int *);
    using FnNvmlDeviceGetDecoderUtilization = NvmlReturn (*)(NvmlDevice, unsigned int *, unsigned int *);
    using FnNvmlDeviceGetPcieThroughput = NvmlReturn (*)(NvmlDevice, unsigned int, unsigned int *);
    using FnNvmlDeviceGetTemperature = NvmlReturn (*)(NvmlDevice, unsigned int, unsigned int *);

    struct NvmlProcessInfo_v2
    {
        unsigned int pid;
        uint64_t     usedGpuMemory;
        unsigned int gpuInstanceId;
        unsigned int computeInstanceId;
    };

    struct NvmlProcessUtilizationSample
    {
        unsigned int pid;
        uint64_t     timeStamp;
        unsigned int smUtil;
        unsigned int memUtil;
        unsigned int encUtil;
        unsigned int decUtil;
    };

    using FnNvmlDeviceGetGraphicsRunningProcesses = NvmlReturn (*)(NvmlDevice, unsigned int *, NvmlProcessInfo_v2 *);
    using FnNvmlDeviceGetComputeRunningProcesses = NvmlReturn (*)(NvmlDevice, unsigned int *, NvmlProcessInfo_v2 *);
    using FnNvmlDeviceGetProcessUtilization = NvmlReturn (*)(NvmlDevice, NvmlProcessUtilizationSample *, unsigned int *, uint64_t);

    FnNvmlDeviceGetGraphicsRunningProcesses pNvmlDeviceGetGraphicsRunningProcesses = nullptr;
    FnNvmlDeviceGetComputeRunningProcesses pNvmlDeviceGetComputeRunningProcesses = nullptr;
    FnNvmlDeviceGetProcessUtilization pNvmlDeviceGetProcessUtilization = nullptr;

    FnNvmlInitV2 pNvmlInitV2 = nullptr;
    FnNvmlShutdown pNvmlShutdown = nullptr;
    FnNvmlSystemGetDriverVersion pNvmlSystemGetDriverVersion = nullptr;
    FnNvmlDeviceGetCountV2 pNvmlDeviceGetCountV2 = nullptr;
    FnNvmlDeviceGetHandleByIndexV2 pNvmlDeviceGetHandleByIndexV2 = nullptr;
    FnNvmlDeviceGetName pNvmlDeviceGetName = nullptr;
    FnNvmlDeviceGetUUID pNvmlDeviceGetUUID = nullptr;
    FnNvmlDeviceGetUtilizationRates pNvmlDeviceGetUtilizationRates = nullptr;
    FnNvmlDeviceGetMemoryInfo pNvmlDeviceGetMemoryInfo = nullptr;
    FnNvmlDeviceGetEncoderUtilization pNvmlDeviceGetEncoderUtilization = nullptr;
    FnNvmlDeviceGetDecoderUtilization pNvmlDeviceGetDecoderUtilization = nullptr;
    FnNvmlDeviceGetPcieThroughput pNvmlDeviceGetPcieThroughput = nullptr;
    FnNvmlDeviceGetTemperature pNvmlDeviceGetTemperature = nullptr;

    bool shouldIgnoreDrmGpu(const QString &driverName, const QString &uevent)
    {
        static const QStringList ignoreTokens {
            QStringLiteral("xrdpdev"),
            QStringLiteral("xrdp"),
            QStringLiteral("vmwgfx"), // vmware virtual GPU device
            QStringLiteral("vmware"),
            QStringLiteral("vkms"),
            QStringLiteral("vgem"),
            QStringLiteral("virtio_gpu"),
            QStringLiteral("virtio"),
            QStringLiteral("hyperv_drm"),
            QStringLiteral("simpledrm"),
            QStringLiteral("bochs-drm"),
            QStringLiteral("bochs_drm"),
            QStringLiteral("bochs"),
            QStringLiteral("qxl")
        };

        return Misc::TextContainsAnyToken(driverName, ignoreTokens) || Misc::TextContainsAnyToken(uevent, ignoreTokens);
    }
}

GPU::GPU()
{
    this->detectGpuBackends();
}

GPU::~GPU()
{
    this->unloadGpuBackends();
}

const GPU::GPUInfo &GPU::FromIndex(int gpuIndex) const
{
    if (gpuIndex < 0 || gpuIndex >= static_cast<int>(this->m_gpus.size()))
        return this->m_nullGPU;
    return *this->m_gpus.at(gpuIndex);
}

GPU::GPUEngineInfo *GPU::GPUInfo::FindEngine(const QString &key)
{
    for (const auto &engine : this->Engines)
    {
        if (engine->Key == key)
            return engine.get();
    }

    return nullptr;
}

void GPU::zeroMissingEngines(GPUInfo &gpu, const QSet<QString> &seenEngineKeys)
{
    for (const auto &engine : gpu.Engines)
    {
        if (seenEngineKeys.contains(engine->Key))
            continue;

        engine->Pct = 0.0;
        engine->History.Push(0.0);
    }
}

void GPU::detectGpuBackends()
{
    LOG_DEBUG("Detecting GPU backends");

    this->m_hasNvml = false;
    this->m_nvmlLibHandle = nullptr;

    if (CFG->ForceGpuDrm)
    {
        LOG_INFO("GPU backend detection forced to DRM via --force-drm");
        this->detectDrmCards();
        return;
    }

    // Try to load NVML for NVIDIA GPUs
    this->m_nvmlLibHandle = ::dlopen("libnvidia-ml.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (!this->m_nvmlLibHandle)
    {
        LOG_DEBUG("NVML scan: unable to locate libnvidia-ml.so.1, trying fallback to libnvidia-ml.so");
        this->m_nvmlLibHandle = ::dlopen("libnvidia-ml.so", RTLD_LAZY | RTLD_LOCAL);
    }

    if (this->m_nvmlLibHandle)
    {
        pNvmlInitV2 = reinterpret_cast<FnNvmlInitV2>(
            ::dlsym(this->m_nvmlLibHandle, "nvmlInit_v2"));
        pNvmlShutdown = reinterpret_cast<FnNvmlShutdown>(
            ::dlsym(this->m_nvmlLibHandle, "nvmlShutdown"));
        pNvmlSystemGetDriverVersion = reinterpret_cast<FnNvmlSystemGetDriverVersion>(
            ::dlsym(this->m_nvmlLibHandle, "nvmlSystemGetDriverVersion"));
        pNvmlDeviceGetCountV2 = reinterpret_cast<FnNvmlDeviceGetCountV2>(
            ::dlsym(this->m_nvmlLibHandle, "nvmlDeviceGetCount_v2"));
        pNvmlDeviceGetHandleByIndexV2 = reinterpret_cast<FnNvmlDeviceGetHandleByIndexV2>(
            ::dlsym(this->m_nvmlLibHandle, "nvmlDeviceGetHandleByIndex_v2"));
        pNvmlDeviceGetName = reinterpret_cast<FnNvmlDeviceGetName>(
            ::dlsym(this->m_nvmlLibHandle, "nvmlDeviceGetName"));
        pNvmlDeviceGetUUID = reinterpret_cast<FnNvmlDeviceGetUUID>(
            ::dlsym(this->m_nvmlLibHandle, "nvmlDeviceGetUUID"));
        pNvmlDeviceGetUtilizationRates = reinterpret_cast<FnNvmlDeviceGetUtilizationRates>(
            ::dlsym(this->m_nvmlLibHandle, "nvmlDeviceGetUtilizationRates"));
        pNvmlDeviceGetMemoryInfo = reinterpret_cast<FnNvmlDeviceGetMemoryInfo>(
            ::dlsym(this->m_nvmlLibHandle, "nvmlDeviceGetMemoryInfo"));
        pNvmlDeviceGetEncoderUtilization = reinterpret_cast<FnNvmlDeviceGetEncoderUtilization>(
            ::dlsym(this->m_nvmlLibHandle, "nvmlDeviceGetEncoderUtilization"));
        pNvmlDeviceGetDecoderUtilization = reinterpret_cast<FnNvmlDeviceGetDecoderUtilization>(
            ::dlsym(this->m_nvmlLibHandle, "nvmlDeviceGetDecoderUtilization"));
        pNvmlDeviceGetPcieThroughput = reinterpret_cast<FnNvmlDeviceGetPcieThroughput>(
            ::dlsym(this->m_nvmlLibHandle, "nvmlDeviceGetPcieThroughput"));
        pNvmlDeviceGetTemperature = reinterpret_cast<FnNvmlDeviceGetTemperature>(
            ::dlsym(this->m_nvmlLibHandle, "nvmlDeviceGetTemperature"));
        pNvmlDeviceGetGraphicsRunningProcesses = reinterpret_cast<FnNvmlDeviceGetGraphicsRunningProcesses>(
            ::dlsym(this->m_nvmlLibHandle, "nvmlDeviceGetGraphicsRunningProcesses_v3"));
        pNvmlDeviceGetComputeRunningProcesses = reinterpret_cast<FnNvmlDeviceGetComputeRunningProcesses>(
            ::dlsym(this->m_nvmlLibHandle, "nvmlDeviceGetComputeRunningProcesses_v3"));
        pNvmlDeviceGetProcessUtilization = reinterpret_cast<FnNvmlDeviceGetProcessUtilization>(
            ::dlsym(this->m_nvmlLibHandle, "nvmlDeviceGetProcessUtilization"));

        const bool symbolsOk = pNvmlInitV2 && pNvmlShutdown && pNvmlDeviceGetCountV2
                               && pNvmlDeviceGetHandleByIndexV2 && pNvmlDeviceGetName
                               && pNvmlDeviceGetUUID && pNvmlDeviceGetUtilizationRates
                               && pNvmlDeviceGetMemoryInfo;
        if (!symbolsOk)
        {
            LOG_DEBUG("NVML: required symbols not found, unloading");
            this->unloadGpuBackends();
        } else if (pNvmlInitV2() != NVML_SUCCESS)
        {
            LOG_DEBUG("NVML: nvmlInit_v2 failed, unloading");
            this->unloadGpuBackends();
        } else
        {
            unsigned int count = 0;
            if (pNvmlDeviceGetCountV2(&count) == NVML_SUCCESS && count > 0)
            {
                this->m_hasNvml = true;
            } else
            {
                LOG_DEBUG("NVML: no devices found, shutting down");
                pNvmlShutdown();
                this->unloadGpuBackends();
            }
        }
    } else
    {
        LOG_DEBUG("NVML scan: no nvidia driver libs detected");
    }

    // DRM sysfs fallback (amdgpu, i915, …); NVIDIA cards covered by NVML are skipped.
    this->detectDrmCards();
}

bool GPU::Sample()
{
    bool ok = false;
    if (this->m_hasNvml)
        ok |= this->sampleNvml();
    if (!this->m_drmCards.isEmpty())
        ok |= this->sampleDrm();
    return ok;
}

void GPU::unloadGpuBackends()
{
    if (this->m_hasNvml && pNvmlShutdown)
        pNvmlShutdown();

    this->m_hasNvml = false;

    pNvmlInitV2 = nullptr;
    pNvmlShutdown = nullptr;
    pNvmlSystemGetDriverVersion = nullptr;
    pNvmlDeviceGetCountV2 = nullptr;
    pNvmlDeviceGetHandleByIndexV2 = nullptr;
    pNvmlDeviceGetName = nullptr;
    pNvmlDeviceGetUUID = nullptr;
    pNvmlDeviceGetUtilizationRates = nullptr;
    pNvmlDeviceGetMemoryInfo = nullptr;
    pNvmlDeviceGetEncoderUtilization = nullptr;
    pNvmlDeviceGetDecoderUtilization = nullptr;
    pNvmlDeviceGetPcieThroughput = nullptr;
    pNvmlDeviceGetTemperature = nullptr;
    pNvmlDeviceGetGraphicsRunningProcesses = nullptr;
    pNvmlDeviceGetComputeRunningProcesses = nullptr;
    pNvmlDeviceGetProcessUtilization = nullptr;

    if (this->m_nvmlLibHandle)
    {
        ::dlclose(this->m_nvmlLibHandle);
        this->m_nvmlLibHandle = nullptr;
    }
}

bool GPU::sampleNvml()
{
    if (!this->m_hasNvml || !pNvmlDeviceGetCountV2 || !pNvmlDeviceGetHandleByIndexV2)
        return false;

    unsigned int count = 0;
    if (pNvmlDeviceGetCountV2(&count) != NVML_SUCCESS)
        return false;

    char driverVer[96] = {};
    if (pNvmlSystemGetDriverVersion)
        pNvmlSystemGetDriverVersion(driverVer, sizeof(driverVer));
    const QString driverVersion = QString::fromLatin1(driverVer).trimmed();

    QSet<QString> seenIds;

    for (unsigned int i = 0; i < count; ++i)
    {
        static constexpr unsigned int kNvmlPcieTxBytes = 0;
        static constexpr unsigned int kNvmlPcieRxBytes = 1;
        static constexpr unsigned int kNvmlTemperatureGpu = 0;

        NvmlDevice dev = nullptr;
        if (pNvmlDeviceGetHandleByIndexV2(i, &dev) != NVML_SUCCESS || !dev)
            continue;

        char nameBuf[128] = {};
        char uuidBuf[96] = {};
        pNvmlDeviceGetName(dev, nameBuf, sizeof(nameBuf));
        pNvmlDeviceGetUUID(dev, uuidBuf, sizeof(uuidBuf));

        const QString name = QString::fromLatin1(nameBuf).trimmed();
        QString id = QString::fromLatin1(uuidBuf).trimmed();
        if (id.isEmpty())
            id = QString("gpu-%1").arg(i);

        NvmlUtilization util{};
        NvmlMemory mem{};
        const bool hasUtil = (pNvmlDeviceGetUtilizationRates(dev, &util) == NVML_SUCCESS);
        const bool hasMem = (pNvmlDeviceGetMemoryInfo(dev, &mem) == NVML_SUCCESS);
        unsigned int txKBps = 0;
        unsigned int rxKBps = 0;
        const bool hasTx = pNvmlDeviceGetPcieThroughput
                           && (pNvmlDeviceGetPcieThroughput(dev, kNvmlPcieTxBytes, &txKBps) == NVML_SUCCESS);
        const bool hasRx = pNvmlDeviceGetPcieThroughput
                           && (pNvmlDeviceGetPcieThroughput(dev, kNvmlPcieRxBytes, &rxKBps) == NVML_SUCCESS);
        unsigned int tempC = 0;
        const bool hasTemp = pNvmlDeviceGetTemperature
                             && (pNvmlDeviceGetTemperature(dev, kNvmlTemperatureGpu, &tempC) == NVML_SUCCESS);

        int gpuIdx = -1;
        for (int j = 0; j < static_cast<int>(this->m_gpus.size()); ++j)
        {
            if (this->m_gpus.at(j)->ID == id)
            {
                gpuIdx = j;
                break;
            }
        }
        if (gpuIdx < 0)
        {
            auto gNew = std::make_unique<GPUInfo>();
            gNew->ID = id;
            this->m_gpus.push_back(std::move(gNew));
            gpuIdx = this->m_gpus.size() - 1;
        }

        GPUInfo &g = *this->m_gpus[gpuIdx];
        g.ID = id;
        g.Name = name;
        g.DriverVersion = driverVersion;
        g.Backend = "NVML";
        g.UtilPct = hasUtil ? qBound(0.0, static_cast<double>(util.gpu), 100.0) : 0.0;
        g.TemperatureC = hasTemp ? static_cast<int>(tempC) : -1;
        g.MemUsedMiB = hasMem ? static_cast<qint64>(mem.used / (1024ULL * 1024ULL)) : 0;
        g.MemTotalMiB = hasMem ? static_cast<qint64>(mem.total / (1024ULL * 1024ULL)) : 0;
        g.SharedMemUsedMiB = 0;
        g.SharedMemTotalMiB = 0;
        g.CopyTxBps = hasTx ? static_cast<double>(txKBps) * 1024.0 : 0.0;
        g.CopyRxBps = hasRx ? static_cast<double>(rxKBps) * 1024.0 : 0.0;
        g.UtilHistory.Push(g.UtilPct);
        const double memPct = (g.MemTotalMiB > 0)
                                  ? (static_cast<double>(g.MemUsedMiB) / static_cast<double>(g.MemTotalMiB)) * 100.0
                                  : 0.0;
        g.MemUsageHistory.Push(memPct);
        g.SharedMemHistory.Push(0.0);
        Misc::PushHistoryAndUpdateMax(g.CopyTxHistory, g.CopyTxBps, g.MaxCopyBps);
        Misc::PushHistoryAndUpdateMax(g.CopyRxHistory, g.CopyRxBps, g.MaxCopyBps);

        QSet<QString> seenEngineKeys;
        auto addEngine = [&](const QString &key, const QString &label, double pct)
        {
            GPUEngineInfo *eng = g.FindEngine(key);
            if (!eng)
            {
                auto newEngine = std::make_unique<GPUEngineInfo>();
                newEngine->Key = key;
                newEngine->Label = label;
                g.Engines.push_back(std::move(newEngine));
                eng = g.Engines.back().get();
            }
            eng->Pct = qBound(0.0, pct, 100.0);
            eng->History.Push(eng->Pct);
            seenEngineKeys.insert(key);
        };

        if (hasUtil)
        {
            double graphicsUtil = util.gpu;
            double computeUtil = util.gpu;

            if (pNvmlDeviceGetGraphicsRunningProcesses
                && pNvmlDeviceGetComputeRunningProcesses
                && pNvmlDeviceGetProcessUtilization)
            {
                auto collectPids = [&dev](auto fn) -> QSet<unsigned int>
                {
                    unsigned int count = 0;
                    fn(dev, &count, nullptr);
                    QSet<unsigned int> pids;
                    if (count > 0)
                    {
                        QVector<NvmlProcessInfo_v2> procs(static_cast<int>(count));
                        if (fn(dev, &count, procs.data()) == NVML_SUCCESS)
                        {
                            for (unsigned int i = 0; i < count; ++i)
                                pids.insert(procs[static_cast<int>(i)].pid);
                        }
                    }
                    return pids;
                };

                QSet<unsigned int> gfxPids  = collectPids(pNvmlDeviceGetGraphicsRunningProcesses);
                QSet<unsigned int> compPids = collectPids(pNvmlDeviceGetComputeRunningProcesses);

                unsigned int procUtilCount = 32;
                QVector<NvmlProcessUtilizationSample> procUtil(static_cast<int>(procUtilCount));
                NvmlReturn ret = pNvmlDeviceGetProcessUtilization(dev, procUtil.data(), &procUtilCount, 0);
                if (ret != NVML_SUCCESS && procUtilCount > 32 && procUtilCount <= 1024)
                {
                    procUtil.resize(static_cast<int>(procUtilCount));
                    ret = pNvmlDeviceGetProcessUtilization(dev, procUtil.data(), &procUtilCount, 0);
                }
                if (ret == NVML_SUCCESS)
                {
                    double sumGfx = 0.0;
                    double sumComp = 0.0;
                    for (unsigned int p = 0; p < procUtilCount; ++p)
                    {
                        const auto &s = procUtil[static_cast<int>(p)];
                        if (compPids.contains(s.pid) && !gfxPids.contains(s.pid))
                            sumComp += s.smUtil;
                        else
                            sumGfx += s.smUtil;
                    }
                    graphicsUtil = qBound(0.0, sumGfx, 100.0);
                    computeUtil = qBound(0.0, sumComp, 100.0);
                }
            }

            addEngine("3d", "3D", graphicsUtil);
            addEngine("cuda", "CUDA", computeUtil);
            addEngine("copy", "Copy", util.memory);
        }
        if (pNvmlDeviceGetEncoderUtilization)
        {
            unsigned int enc = 0, sampling = 0;
            if (pNvmlDeviceGetEncoderUtilization(dev, &enc, &sampling) == NVML_SUCCESS)
                addEngine("video-encode", "Video Encode", enc);
        }
        if (pNvmlDeviceGetDecoderUtilization)
        {
            unsigned int dec = 0, sampling = 0;
            if (pNvmlDeviceGetDecoderUtilization(dev, &dec, &sampling) == NVML_SUCCESS)
                addEngine("video-decode", "Video Decode", dec);
        }

        this->zeroMissingEngines(g, seenEngineKeys);
        seenIds.insert(id);
    }
    // Zero-out stale NVML GPUs that disappeared between ticks (e.g. hot-unplug).
    // Only touch entries that were previously produced by NVML (backend == "nvml").
    for (const auto &g : this->m_gpus)
    {
        if (g->Backend != QLatin1String("NVML"))
            continue;
        if (!seenIds.contains(g->ID))
        {
            g->UtilPct = 0.0;
            g->TemperatureC = -1;
            g->MemUsedMiB = 0;
            g->MemTotalMiB = 0;
            g->SharedMemUsedMiB = 0;
            g->SharedMemTotalMiB = 0;
            g->CopyTxBps = 0.0;
            g->CopyRxBps = 0.0;
            g->UtilHistory.Push(0.0);
            g->MemUsageHistory.Push(0.0);
            g->SharedMemHistory.Push(0.0);
            Misc::PushHistoryAndUpdateMax(g->CopyTxHistory, 0.0, g->MaxCopyBps);
            Misc::PushHistoryAndUpdateMax(g->CopyRxHistory, 0.0, g->MaxCopyBps);
            for (const auto &e : g->Engines)
            {
                e->Pct = 0.0;
                e->History.Push(0.0);
            }
        }
    }
    return true;
}

// ── GPU: DRM sysfs backend (amdgpu, i915, …) ─────────────────────────────────

void GPU::detectDrmCards()
{
    LOG_DEBUG("Detecting DRM GPUs");

    this->m_drmCards.clear();

    const QDir drmDir("/sys/class/drm");
    const QStringList entries = drmDir.entryList({"card[0-9]*"}, QDir::Dirs);

    for (const QString &entry : entries)
    {
        if (entry.contains('-'))   // skip card0-eDP-1, card1-HDMI-A-1, …
            continue;

        const QString devPath = drmDir.filePath(entry + "/device");
        const QString vendorStr = Misc::ReadFile(devPath + "/vendor").trimmed();
        const QString uevent = Misc::ReadFile(devPath + "/uevent");

        // Skip NVIDIA cards already managed by NVML to avoid duplicate entries.
        if (this->m_hasNvml && vendorStr == QLatin1String("0x10de"))
            continue;

        DRMCard card;
        card.Vendor = vendorStr;
        card.CardNodePath = QStringLiteral("/dev/dri/") + entry;

        // Stable identifier: the PCI address resolved from the sysfs symlink.
        const QString canonical = QFileInfo(devPath).canonicalFilePath();
        card.ID = QFileInfo(canonical).fileName();
        if (card.ID.isEmpty())
            card.ID = entry;

        // Detect the render node for this card (used by fdinfo engine scanner).
        const QStringList renderNodes = QDir(devPath + "/drm").entryList({"renderD*"}, QDir::Dirs);
        if (!renderNodes.isEmpty())
            card.RenderNodePath = QStringLiteral("/dev/dri/") + renderNodes.first();

        // First hwmon sub-directory holds temperature and driver name.
        const QStringList hwmons = QDir(devPath + "/hwmon").entryList({"hwmon[0-9]*"}, QDir::Dirs);
        if (!hwmons.isEmpty())
        {
            const QString hwPath = devPath + "/hwmon/" + hwmons.first();
            card.DriverName = Misc::ReadFile(hwPath + "/name").trimmed();
            const QString tp = hwPath + "/temp1_input";
            if (QFileInfo::exists(tp))
                card.TempPath = tp;
        }

        if (card.DriverName.isEmpty())
            card.DriverName = Misc::FileNameFromSymlink(devPath + "/driver/module");
        if (card.DriverName.isEmpty())
            card.DriverName = Misc::FileNameFromSymlink(devPath + "/driver");

        if (shouldIgnoreDrmGpu(card.DriverName, uevent))
            continue;

        // Driver version: try module version first, fall back to kernel version
        // for in-tree drivers (e.g. amdgpu ships inside the kernel).
        if (!card.DriverName.isEmpty())
        {
            const QString ver = Misc::ReadFile("/sys/module/" + card.DriverName + "/version").trimmed();
            if (!ver.isEmpty())
                card.DriverVersion = ver;
        }
        if (card.DriverVersion.isEmpty())
            card.DriverVersion = QSysInfo::kernelVersion();

        const QString busyPath = devPath + "/gpu_busy_percent";
        if (QFileInfo::exists(busyPath))
            card.BusyPath = busyPath;

        // Dynamically detect all *_busy_percent engine files.
        const QStringList busyFiles = QDir(devPath).entryList({"*_busy_percent"}, QDir::Files);
        for (const QString &f : busyFiles)
        {
            if (f == QLatin1String("gpu_busy_percent"))
                continue;  // handled separately as overall utilisation

            static const QLatin1String suffix("_busy_percent");
            const QString key = f.chopped(suffix.size());
            card.EngineBusyPaths.append({key, devPath + "/" + f});
        }

        const QString vramT = devPath + "/mem_info_vram_total";
        const QString vramU = devPath + "/mem_info_vram_used";
        if (QFileInfo::exists(vramT) && QFileInfo::exists(vramU))
        {
            card.VramTotalPath = vramT;
            card.VramUsedPath  = vramU;
        }

        const QString gttT = devPath + "/mem_info_gtt_total";
        const QString gttU = devPath + "/mem_info_gtt_used";
        if (QFileInfo::exists(gttT) && QFileInfo::exists(gttU))
        {
            card.GttTotalPath = gttT;
            card.GttUsedPath  = gttU;
        }

        LOG_DEBUG("Found DRM GPU: " + entry);
        this->m_drmCards.append(card);
    }
}

bool GPU::sampleDrm()
{
    const qint64 fdInfoElapsedNs = this->m_gpuFdInfoTimerStarted
                                       ? this->m_gpuFdInfoTimer.nsecsElapsed()
                                       : 0;

    ++this->m_gpuFdInfoRescanCounter;

    for (DRMCard &card : this->m_drmCards)
    {
        int gpuIdx = -1;
        for (int j = 0; j < static_cast<int>(this->m_gpus.size()); ++j)
        {
            if (this->m_gpus.at(j)->ID == card.ID)
            {
                gpuIdx = j;
                break;
            }
        }

        if (gpuIdx < 0)
        {
            auto g = std::make_unique<GPUInfo>();
            g->ID            = card.ID;
            g->DriverVersion = card.DriverVersion;
            g->Backend       = card.DriverName.isEmpty()
                            ? QStringLiteral("drm") : card.DriverName;

            if (card.Vendor == QLatin1String("0x1002"))
                g->Name = QStringLiteral("AMD Radeon");
            else if (card.Vendor == QLatin1String("0x8086"))
                g->Name = QStringLiteral("Intel Graphics");
            else
                g->Name = QStringLiteral("GPU");

            this->m_gpus.push_back(std::move(g));
            gpuIdx = this->m_gpus.size() - 1;
        }

        GPUInfo &g = *this->m_gpus[gpuIdx];
        g.TemperatureC = -1;

        if (!card.BusyPath.isEmpty())
        {
            bool ok = false;
            const int pct = Misc::ReadFile(card.BusyPath).trimmed().toInt(&ok);
            g.UtilPct = ok ? qBound(0.0, static_cast<double>(pct), 100.0) : 0.0;
        } else
        {
            g.UtilPct = 0.0;
        }
        g.UtilHistory.Push(g.UtilPct);

        if (!card.TempPath.isEmpty())
        {
            bool ok = false;
            const int milliC = Misc::ReadFile(card.TempPath).trimmed().toInt(&ok);
            g.TemperatureC = ok ? milliC / 1000 : -1;
        }

        if (!card.VramTotalPath.isEmpty())
        {
            bool okT = false, okU = false;
            const qint64 total = Misc::ReadFile(card.VramTotalPath).trimmed().toLongLong(&okT);
            const qint64 used  = Misc::ReadFile(card.VramUsedPath).trimmed().toLongLong(&okU);
            g.MemTotalMiB = okT ? total / (1024LL * 1024LL) : 0;
            g.MemUsedMiB  = okU ? used  / (1024LL * 1024LL) : 0;
        }

        if (!card.GttTotalPath.isEmpty())
        {
            bool okT = false, okU = false;
            const qint64 total = Misc::ReadFile(card.GttTotalPath).trimmed().toLongLong(&okT);
            const qint64 used  = Misc::ReadFile(card.GttUsedPath).trimmed().toLongLong(&okU);
            g.SharedMemTotalMiB = okT ? total / (1024LL * 1024LL) : 0;
            g.SharedMemUsedMiB  = okU ? used  / (1024LL * 1024LL) : 0;
        }

        const double memPct = (g.MemTotalMiB > 0)
                                  ? static_cast<double>(g.MemUsedMiB)
                                        / static_cast<double>(g.MemTotalMiB) * 100.0
                                  : 0.0;
        g.MemUsageHistory.Push(memPct);

        const double sharedPct = (g.SharedMemTotalMiB > 0)
                                     ? static_cast<double>(g.SharedMemUsedMiB)
                                           / static_cast<double>(g.SharedMemTotalMiB) * 100.0
                                     : 0.0;
        g.SharedMemHistory.Push(sharedPct);

        g.CopyTxHistory.Push(0.0);
        g.CopyRxHistory.Push(0.0);

        // ── Engine data ──────────────────────────────────────────────────────
        QSet<QString> seenEngineKeys;
        auto addEngine = [&](const QString &key, const QString &label, double pct)
        {
            GPUEngineInfo *eng = g.FindEngine(key);
            if (!eng)
            {
                auto newEngine = std::make_unique<GPUEngineInfo>();
                newEngine->Key = key;
                newEngine->Label = label;
                g.Engines.push_back(std::move(newEngine));
                eng = g.Engines.back().get();
            }
            eng->Pct = qBound(0.0, pct, 100.0);
            eng->History.Push(eng->Pct);
            seenEngineKeys.insert(key);
        };

        if (!card.BusyPath.isEmpty())
            addEngine("gfx", "GFX", g.UtilPct);

        // Dynamic sysfs engines (vcn_busy_percent, jpeg_busy_percent, …).
        for (const auto &ep : std::as_const(card.EngineBusyPaths))
        {
            bool ok = false;
            const int pct = Misc::ReadFile(ep.second).trimmed().toInt(&ok);
            addEngine(ep.first, ep.first.toUpper(), ok ? static_cast<double>(pct) : 0.0);
        }

        // fdinfo-based engines (Compute, etc.) — cumulative ns, need delta.
        if (!card.RenderNodePath.isEmpty())
        {
            const QHash<QString, qint64> curNs = this->scanDrmFdInfoEngines(card);
            QSet<QString> sysFsKeys;
            sysFsKeys.insert(QStringLiteral("gfx"));
            for (const auto &ep : std::as_const(card.EngineBusyPaths))
                sysFsKeys.insert(ep.first);

            for (auto it = curNs.cbegin(); it != curNs.cend(); ++it)
            {
                if (sysFsKeys.contains(it.key()))
                    continue;                       // already covered by sysfs
                double pct = 0.0;
                if (fdInfoElapsedNs > 0 && g.PrevFDInfoEngineNs.contains(it.key()))
                {
                    const qint64 delta = it.value() - g.PrevFDInfoEngineNs.value(it.key());
                    pct = static_cast<double>(delta) / static_cast<double>(fdInfoElapsedNs) * 100.0;
                }
                QString label = it.key();
                for (int ci = 0; ci < label.size(); ++ci)
                {
                    if (ci == 0 || (ci > 0 && label[ci - 1] == QChar('-')))
                        label[ci] = label[ci].toUpper();
                }
                addEngine(it.key(), label, pct);
            }
            g.PrevFDInfoEngineNs = curNs;
        }
        this->zeroMissingEngines(g, seenEngineKeys);
    }

    // Restart the fdinfo timer after all DRM cards are sampled.
    this->m_gpuFdInfoTimer.start();
    this->m_gpuFdInfoTimerStarted = true;

    return !this->m_drmCards.isEmpty();
}

// ── GPU: fdinfo engine scanner ────────────────────────────────────────────────
// Reads drm-engine-* nanosecond values from /proc fdinfo files.
// Caches discovered fdinfo paths and only does a full /proc rescan every few ticks.
// De-duplicates by drm-client-id.

QHash<QString, qint64> GPU::scanDrmFdInfoEngines(DRMCard &card)
{
    QHash<QString, qint64> totals;
    QSet<int> seenClients;

    // Helper: parse a single fdinfo file and accumulate engine nanosecond values.
    auto parseFdInfo = [&](const QString &infoPath) -> bool
    {
        const QString content = Misc::ReadFile(infoPath);
        if (content.isEmpty())
            return false;

        if (!content.contains(QLatin1String("drm-pdev:\t") + card.ID)
            && !content.contains(QLatin1String("drm-pdev: ") + card.ID))
            return false;

        int clientId = -1;
        for (const auto &line : QStringView(content).split('\n'))
        {
            if (line.startsWith(QLatin1String("drm-client-id:")))
            {
                clientId = line.mid(line.indexOf(':') + 1).trimmed().toInt();
                break;
            }
        }
        if (clientId >= 0 && seenClients.contains(clientId))
            return false;
        if (clientId >= 0)
            seenClients.insert(clientId);

        bool found = false;
        for (const auto &line : QStringView(content).split('\n'))
        {
            if (!line.startsWith(QLatin1String("drm-engine-")))
                continue;
            const int colonPos = line.indexOf(':');
            if (colonPos < 0)
                continue;
            const QString key = line.mid(11, colonPos - 11).toString();
            const QStringView valStr = line.mid(colonPos + 1).trimmed();
            const int spacePos = valStr.indexOf(' ');
            const qint64 ns = (spacePos > 0 ? valStr.left(spacePos) : valStr).toLongLong();
            totals[key] += ns;
            found = true;
        }
        return found;
    };

    const bool fullRescan = (this->m_gpuFdInfoRescanCounter % 5 == 1) || card.CachedFDInfoPaths.isEmpty();

    if (!fullRescan)
    {
        // Fast path: only re-read previously discovered fdinfo paths.
        QStringList stillValid;
        for (const QString &path : std::as_const(card.CachedFDInfoPaths))
        {
            if (parseFdInfo(path))
                stillValid.append(path);
        }
        card.CachedFDInfoPaths = stillValid;
        return totals;
    }

    // Full rescan: walk /proc/*/fd to discover new fdinfo paths.
    QStringList newCache;

    const QDir procDir(QStringLiteral("/proc"));
    const QStringList pids = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString &pidEntry : pids)
    {
        bool ok = false;
        pidEntry.toInt(&ok);
        if (!ok)
            continue;

        const QString fdDirPath = QStringLiteral("/proc/") + pidEntry + QStringLiteral("/fd");
        const QDir fdDir(fdDirPath);
        if (!fdDir.exists())
            continue;

        const QStringList fdEntries = fdDir.entryList(QDir::NoDotAndDotDot);
        for (const QString &fdNum : fdEntries)
        {
            const QString linkPath = fdDirPath + QChar('/') + fdNum;
            char buf[PATH_MAX];
            const ssize_t len = ::readlink(linkPath.toLocal8Bit().constData(), buf, sizeof(buf) - 1);
            if (len <= 0)
                continue;
            buf[len] = '\0';
            const QByteArray target(buf, static_cast<int>(len));

            if (target != card.RenderNodePath.toLatin1()
                && target != card.CardNodePath.toLatin1())
                continue;

            const QString infoPath = QStringLiteral("/proc/") + pidEntry + QStringLiteral("/fdinfo/") + fdNum;
            if (parseFdInfo(infoPath))
                newCache.append(infoPath);
        }
    }

    card.CachedFDInfoPaths = newCache;
    return totals;
}
