# AMD SMI Backend Notes

This project supports AMD GPUs through a runtime-loaded AMD SMI backend in:

- [src/system/gpu/amdsmibackend.h](/shared/sandbox/SystemInfo/src/system/gpu/amdsmibackend.h)
- [src/system/gpu/amdsmibackend.cpp](/shared/sandbox/SystemInfo/src/system/gpu/amdsmibackend.cpp)

## Purpose

The goal is the same as the NVML backend:

- use the vendor API when it is available
- avoid a hard build/runtime dependency when it is not installed
- keep the application working normally on systems without ROCm

The backend therefore uses `dlopen()` / `dlsym()` and never links against AMD SMI at build time.

## Backend Order

GPU backend detection currently works in this order:

1. `NVML` for NVIDIA GPUs
2. `AMD SMI` for AMD GPUs
3. `DRM` fallback for anything not covered by vendor APIs

When AMD SMI is active, DRM skips AMD GPUs to avoid duplicate entries.
When NVML is active, DRM skips NVIDIA GPUs for the same reason.

## Library Loading

The AMD SMI backend currently tries:

1. `libamd_smi.so`
2. `libamd_smi.so.1`

If neither is present, the backend stays disabled and the application falls back to DRM.

This mirrors the current NVML approach.

## Metrics Used

The current implementation intentionally uses a small subset of the AMD SMI API:

- device discovery through sockets and processor handles
- GPU identity through BDF and optional UUID
- name through processor info
- GPU activity through `amdsmi_get_gpu_activity()`
- temperature through `amdsmi_get_temp_metric()`
- power through `amdsmi_get_power_info()`
- core clock through `amdsmi_get_clock_info()`
- VRAM used/total through `amdsmi_get_gpu_vram_usage()`

These values are mapped into the shared `GPU::GPUInfo` structure, the same way the NVML and DRM backends populate it.

At the moment the AMD SMI backend provides:

- `UtilPct`
- `TemperatureC`
- `CoreClockMHz`
- `PowerUsageW`
- `MemUsedMiB`
- `MemTotalMiB`
- basic engine rows:
  - `GFX`
  - `MEM`
  - `Media`

It does not currently populate:

- shared/system GPU memory
- PCIe throughput
- per-process GPU stats
- richer media/clock domains beyond the basic values above

## Why This Is Kept Small

AMD SMI has a larger API surface and evolving structs. For this first implementation, the backend uses the smaller monitoring calls instead of depending on the larger metrics struct layout everywhere.

That keeps the implementation simpler and reduces ABI assumptions while still giving us the metrics we need for the current UI.

## References

These are the main AMD sources used while implementing the backend:

- AMD SMI C++ usage and dynamic library example:
  https://rocm.docs.amd.com/projects/amdsmi/en/latest/how-to/amdsmi-cpp-lib.html

- AMD SMI C header/API reference:
  https://rocm.docs.amd.com/projects/amdsmi/en/docs-6.2.1/doxygen/docBin/html/amdsmi_8h.html

- AMD SMI GPU monitoring APIs:
  https://rocm.docs.amd.com/projects/amdsmi/en/docs-7.1.0/doxygen/docBin/html/group__tagGPUMonitor.html

- `amdsmi_engine_usage_t` fields:
  https://rocm.docs.amd.com/projects/amdsmi/en/docs-7.1.0/doxygen/docBin/html/structamdsmi__engine__usage__t.html

- `amdsmi_power_info_t` fields:
  https://rocm.docs.amd.com/projects/amdsmi/en/docs-7.2.0/doxygen/docBin/html/structamdsmi__power__info__t.html

- `amdsmi_vram_usage_t` fields:
  https://rocm.docs.amd.com/projects/amdsmi/en/docs-6.3.1/doxygen/docBin/html/structamdsmi__vram__usage__t.html

- AMD SMI GPU metrics overview:
  https://rocm.docs.amd.com/projects/amdsmi/en/develop/doxygen/docBin/html/structamdsmi__gpu__metrics__t.html

- AMD SMI Python reference for `get_clock_info()` field descriptions:
  https://rocm.docs.amd.com/projects/amdsmi/en/docs-7.0.2/reference/amdsmi-py-api.html

## Caveat

This backend was implemented against AMD's published documentation, not against locally installed ROCm headers on the current development machine.

That means:

- the implementation is intentionally conservative
- if AMD changes ABI details across ROCm releases, the backend may need adjustment
- validating against a machine with ROCm and `libamd_smi.so` installed is still recommended
