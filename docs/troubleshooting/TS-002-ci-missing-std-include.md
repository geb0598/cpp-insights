---
id: TS-002
date: 2026-11-03
area: Build
symptoms: [C2039, "'sort' is not a member of 'std'", builds locally but fails in CI, missing include]
fix_commit: e115e73
issue: 2
---

# TS-002: CI fails with C2039 on code that builds locally

## Symptom

The first CI run failed to compile `tests/integration/game_stub/main.cpp` with
`C2039: 'sort' is not a member of 'std'`. The same commit built cleanly on the
maintainer's machine.

## Environment

Local MSVC 19.36; the `windows-2022` GitHub runner has MSVC 19.44.

## Cause

The file used `std::sort` without including `<algorithm>`. MSVC 19.36's standard
headers pulled `<algorithm>` in through other includes; 19.44's do not. Code that
relies on transitive includes builds or breaks depending on the toolset version.

## Fix

Include the header for each standard facility used directly: `<algorithm>` in
`game_stub/main.cpp`, `timeline_panel.cpp`, and `toolbar_panel.cpp`, and `<cmath>`
in `timeline_panel.cpp`. The two viewer files still built in CI, but only through
transitive includes.

## Verify

CI builds the `debug` preset on `windows-2022`. A local build with an older
toolset does not prove a missing include is safe.
