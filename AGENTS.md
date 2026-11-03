# AGENTS.md

cpp-insights is a Windows-only C++17 profiler. A client SDK linked into a game
records per-frame CPU and GPU (D3D11) scope timings and streams them over named
pipes to a DirectX 11 + ImGui viewer.

## Commands

```bash
cmake --preset debug                                       # tests and stubs build only in debug
cmake --build build/debug --config Debug
ctest --test-dir build/debug -C Debug --output-on-failure
build/debug/Debug/insights-test.exe --gtest_filter=Suite.Name
```

The D3D11 backend, `Connection`, and `Client`/`Server` have no automated tests.
Verify them by hand: start `build/debug/viewer/Debug/cpp-insights-viewer.exe`,
then `build/debug/tests/integration/gpu_stub/Debug/gpu-stub.exe`, press record,
and inspect the flame graph. Say what you could not verify.

## Repository map

```
include/insights/      public SDK headers (shipped); gpu/ holds the GPU profiler
src/insights/          implementation, including the viewer-side Server and Reporter
viewer/                viewer application (ImGui panels)
tests/unit/            GoogleTest suites (src/ is on the include path)
tests/integration/     game_stub, gpu_stub driver apps (loop forever, not in CTest)
scripts/make_release.py  interactive release packaging into ../releases/<version>/
docs/troubleshooting/  solved problems: index plus one file per case
```

## Gotchas

- Every public macro is a no-op unless the consumer defines `INSIGHTS`, and
  `INSIGHTS_GPU` for GPU macros. Linking the CMake targets defines neither.
- Threads: the *frame thread* is the app thread that calls
  `INSIGHTS_FRAME_BEGIN/END`; it alone may use the D3D11 immediate context.
  Packet callbacks run on `Connection` worker threads; hand work to the frame
  thread through an atomic flag consumed at the next frame boundary, and keep
  viewer-side shared state in `Reporter` under its mutex.
- `ScopeRecord` times are nanoseconds since `ScopeProfiler::GetRecordingStart()`
  on CPU and GPU tracks alike; the flame graph plots them with no alignment.
- `Registry`, `Reporter`, `ScopeProfiler`, `GpuProfiler`, `Client`, `Server` are
  singletons. Tests reset the ones they touch in `SetUp()`.
- Descriptor and group IDs come from static counters that `Clear()` does not
  rewind. Never assert or hard-code a literal ID.
- `Viewer::Render()` indexes `panels_` by position; adding or reordering a panel
  means updating `Init()` and `Render()` together.

## Where to look

| When | Read |
| --- | --- |
| Debugging a failure or odd behavior | Search [docs/troubleshooting/README.md](docs/troubleshooting/README.md) first, then open only the matching case |
| Code style, commits, issues, writing docs | [.github/CONTRIBUTING.md](.github/CONTRIBUTING.md) |
| Anything else | The code. Non-obvious constraints are commented where they apply |

Do not open, edit, or close GitHub issues without the maintainer's approval.
