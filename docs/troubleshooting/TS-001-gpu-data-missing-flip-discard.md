---
id: TS-001
date: 2026-03-30
area: GPU
symptoms: [no GPU scopes, empty GPU track, FLIP_DISCARD, flip model swap chain]
fix_commit: b4dd9bb
issue:
---

# TS-001: GPU data missing with a FLIP_DISCARD swap chain

## Symptom

With a game using `DXGI_SWAP_EFFECT_FLIP_DISCARD`, recording produced CPU scopes
but no GPU scopes.

## Environment

A consumer app with a flip-model swap chain. `gpu_stub` and the viewer use
`DXGI_SWAP_EFFECT_DISCARD`, so the stub does not reproduce this.

## Cause

Inferred; the fix commit has no description. `CollectFrame()` reads a slot
`FRAME_LATENCY` frames after it was written, using
`D3D11_ASYNC_GETDATA_DONOTFLUSH`, and the next `BeginFrame()` reuses that slot.
With `FRAME_LATENCY = 3`, the queries were apparently not complete when read,
which fits the deeper frame queue of flip-model presentation. Every frame was
then dropped.

## Fix

`FRAME_LATENCY` raised from 3 to 5 in `d3d11_gpu_profiler_backend.h`.

## Verify

Switch `gpu_stub`'s swap chain to `DXGI_SWAP_EFFECT_FLIP_DISCARD` (it needs
`BufferCount >= 2`, which it has) and confirm GPU scopes appear in the flame graph.
A fixed latency can still fall short under heavier GPU load, since a frame not
ready in time is dropped without any report.
