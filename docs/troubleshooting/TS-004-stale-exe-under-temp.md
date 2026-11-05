---
id: TS-004
date: 2026-11-05
area: Build
symptoms: [MSB8029, stale executable, change has no effect, test passes after breaking change, not relinked, TEMP]
fix_commit:
issue: 6
---

# TS-004: Rebuild under %TEMP% leaves a stale executable

## Symptom

After editing `game_stub`, a rebuild kept running the old `game-stub.exe`, so a
test meant to fail with the change still passed. Every project in the build
logs warning `MSB8029`: intermediate or output directories should not be under
the temporary directory, because incremental builds can break.

## Environment

A git worktree under `C:\Users\<user>\AppData\Local\Temp\`, used as the short
path suggested for TS-003. The stale binary was a dependency (`game-stub`)
launched by another target (`insights-ipc-test`), not the target being edited.

## Cause

Inferred from `MSB8029`: MSBuild's incremental build checks are unreliable when
the build tree is under `%TEMP%`, and `game-stub` was not relinked. The exact
mechanism was not confirmed.

## Fix

No code change. Build outside `%TEMP%` on a short path, for example
`C:\wt\<branch>`. If you must build under `%TEMP%`, force a full rebuild
(`cmake --build build/debug --config Debug --clean-first`) and check that the
executables' timestamps changed.

## Verify

The build log has no `MSB8029` warnings, and the executable's timestamp changes
after editing one of its sources.
