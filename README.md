<div align="center">
  <img src="viewer/data/cpp-insights.png" alt="cpp-insights" width="80" />
  <h1>cpp-insights</h1>
  <p>A real-time C++ performance profiling framework for games and real-time applications</p>

  [![CI](https://github.com/geb0598/cpp-insights/actions/workflows/ci.yml/badge.svg)](https://github.com/geb0598/cpp-insights/actions/workflows/ci.yml)
  [![Platform](https://img.shields.io/badge/platform-Windows-blue?logo=windows)](https://www.microsoft.com/windows)
  [![C++](https://img.shields.io/badge/C%2B%2B-17-blue?logo=cplusplus)](https://en.cppreference.com)
  [![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)
  [![CMake](https://img.shields.io/badge/build-CMake-red?logo=cmake)](https://cmake.org)

  [한국어](README.ko.md)
</div>

---

![cpp-insights](docs/images/insights-1.png)

## Overview

**cpp-insights** is a lightweight, non-intrusive profiling framework for real-time C++ applications. Instrument your code with a few macros, then visualize live CPU and GPU performance data in the companion viewer — no external dependencies required beyond DirectX 11.

## Features

- **RAII-based scope timing** — Instrument any code path with a single macro. Timing begins and ends automatically with the enclosing scope; no manual start/stop calls required.
- **Non-intrusive async transport** — Frame data is streamed to the viewer over IPC without blocking the game loop. Recording and analysis run in a separate process.
- **CPU & GPU profiling** — CPU scopes and D3D11 GPU scopes are recorded on a unified nanosecond timeline, so CPU and GPU work can be analyzed side by side.
- **Rich statistical analysis** — The viewer provides per-frame breakdown, scrolling frame time monitor, hierarchical flame graph, and a call tree with inclusive / exclusive timing and avg / min / max / p95 / p99 statistics.
- **Session save / load** — Recording sessions can be saved to disk and reopened later for offline analysis.
- **Minimal client overhead** — The client library has no external dependencies. GPU profiling is opt-in via a compile-time flag and adds only a D3D11 link dependency.

## Implementation Notes

- **Two-pipe IPC** — A dedicated data pipe (client → server) carries frame payloads; a separate control pipe (server → client) handles session commands, so data flow and session control never share a channel.
- **Producer-consumer send queue** — `std::mutex` + `std::condition_variable` decouple the game thread from the send worker. The game thread enqueues a frame and returns immediately; a background thread handles transmission.
- **D3D11 timestamp queries** — Per-scope `ID3D11Query` timestamp pairs are managed in a 5-frame circular buffer to absorb GPU pipeline latency. A disjoint query detects GPU context loss and discards corrupted samples.
- **Unified CPU / GPU timeline** — GPU ticks are converted to nanoseconds using the disjoint query frequency and a calibrated base tick, placing CPU and GPU scopes on the same time axis.
- **Custom binary serialization** — A single `Archive` base class unifies serialization and deserialization into one `operator<<` chain via an `IsLoading()` flag, with no external libraries.

## Viewer

![cpp-insights panels](docs/images/insights-2.png)

**① Toolbar** — Manages the connection to a running client, controls recording start / stop, and provides session save / load.

**② Real-time Frame Monitor** — Scrolling frame time graph that updates live during recording. Useful for spotting hitches and frame pacing issues as they happen.

**③ Frame Timeline** — Displays frame execution times in a histogram format. Users can adjust two draggable bars to define a specific frame range, which updates the data displayed in the ⑤ Call Stack panel.

**④ Flame Graph** — Hierarchical CPU & GPU scope visualization across all recorded frames. Hover any scope to see its name, duration, depth, and frame number.

**⑤ Call Stack** — Call tree view with inclusive / exclusive timing and call counts per stat descriptor. Useful for identifying the true cost of nested scopes.

## Requirements

| Component | Minimum |
|-----------|---------|
| OS | Windows 10 / 11 |
| Compiler | MSVC 2019+ |
| DirectX | D3D11 (viewer + GPU profiling) |

## Getting Started

### Option 1 — Download pre-built

Download the latest release from [Releases](https://github.com/geb0598/cpp-insights/releases):

- **`cpp-insights-vX.Y.Z-viewer.zip`** — extract and run `cpp-insights-viewer.exe`
- **`cpp-insights-vX.Y.Z-sdk.zip`** — headers and pre-built libraries for integration

**Visual Studio integration:**

1. Add `sdk/include/` to **Additional Include Directories**
2. Add `sdk/lib/Release/` to **Additional Library Directories**
3. Add `cpp-insights-core.lib` to **Additional Dependencies**
4. Add `INSIGHTS` to **Preprocessor Definitions**

For GPU profiling, also add `cpp-insights-gpu.lib` and `INSIGHTS_GPU` to the preprocessor definitions.

> The pre-built libraries target MSVC 2022 with `/MD` runtime. If you encounter linker issues, use Option 2 to build from source.

### Option 2 — Build from source

```bash
git clone https://github.com/geb0598/cpp-insights.git
cd cpp-insights

cmake --preset release
cmake --build build/release --preset release
```

The viewer will be at `build/release/viewer/Release/cpp-insights-viewer.exe`.

---

## Usage

**Step 1 — Include the appropriate header**

Choose based on whether you need GPU profiling:

```cpp
// CPU profiling only
#include "insights/insights.h"
```

```cpp
// With D3D11 GPU profiling — use this header instead of insights.h
#include "insights/insights_d3d11.h"
```

> Use the same header consistently across all translation units that reference insights macros.

**Step 2 — Declare stat groups and stats** (once, in any translation unit)

```cpp
INSIGHTS_DECLARE_STATGROUP("Rendering",  GRenderGroup);

INSIGHTS_DECLARE_STAT("Draw Calls",  GDrawCallsStat,  GRenderGroup);
INSIGHTS_DECLARE_STAT("Shadow Pass", GShadowStat,     GRenderGroup);
```

**Step 3 — Initialize at startup**

```cpp
// CPU profiling only
INSIGHTS_INITIALIZE();
```

```cpp
// With D3D11 GPU profiling
INSIGHTS_GPU_INIT_D3D11(pDevice, pContext);  // must be called before INSIGHTS_INITIALIZE
INSIGHTS_INITIALIZE();
```

**Step 4 — Instrument your frame loop**

```cpp
while (running) {
    INSIGHTS_FRAME_BEGIN();

    {
        INSIGHTS_SCOPE(GDrawCallsStat);
        // ... your rendering code ...
    }
    {
        INSIGHTS_GPU_SCOPE(GShadowStat);
        // ... GPU work ...
    }

    INSIGHTS_FRAME_END();
}
```

> `INSIGHTS_GPU_SCOPE` issues a D3D11 timestamp query pair on each construction. Prefer pass-level scopes over per-draw-call scopes to keep query overhead low.
>
> ```cpp
> // ❌ Avoid — one query pair per draw call
> for (auto& mesh : meshes) {
>     INSIGHTS_GPU_SCOPE(GMeshStat);
>     DrawMesh(mesh);
> }
>
> // ✅ Prefer — one query pair for the entire pass
> {
>     INSIGHTS_GPU_SCOPE(GMeshStat);
>     for (auto& mesh : meshes) {
>         DrawMesh(mesh);
>     }
> }
> ```

**Step 5 — Shutdown**

```cpp
INSIGHTS_SHUTDOWN();
```

Launch `cpp-insights-viewer.exe` and click **Connect** to begin a live profiling session.

> To disable all profiling with zero overhead (e.g. shipping builds), remove the `INSIGHTS` preprocessor definition. All macros will compile to nothing.

## Roadmap

- [ ] Multi-threaded profiling — `ScopeProfiler` currently assumes single-threaded frame recording

## Acknowledgements

- [JetBrains Mono](https://www.jetbrains.com/lp/mono/) — [SIL Open Font License 1.1](OFL.txt)
- [Dear ImGui](https://github.com/ocornut/imgui) — MIT License
- [ImPlot](https://github.com/epezent/implot) — MIT License

## License

Distributed under the [MIT License](LICENSE). © 2026 geb0598
