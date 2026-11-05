---
id: TS-003
date: 2026-11-03
area: Build
symptoms: [FTK1011, file tracker, configure fails, build fails in a deep folder, path too long]
fix_commit:
issue:
---

# TS-003: Build fails with FTK1011 in a deeply nested folder

## Symptom

`cmake --preset debug` or the build fails with MSBuild FileTracker error
`FTK1011` before any of the project's code compiles. The same commit builds in
the main checkout.

## Environment

A git worktree created inside an agent's scratchpad folder, about 100 characters
deep (`C:\Users\<user>\AppData\Local\Temp\claude\<project>\<session id>\scratchpad\...`).
Paths under `build/debug/_deps` and the `.tlog` files MSBuild writes add another
100 or more.

## Cause

MSBuild's file tracker cannot create its tracking log when the full path exceeds
the Windows path length limit (260 characters). Whether enabling Windows long-path
support avoids it was not tested.

## Fix

No code change. Put the worktree on a short path, and not under `%TEMP%` (see
TS-004), for example `C:\wt\<branch>`.

## Verify

Configure and build the `debug` preset from the new location.
