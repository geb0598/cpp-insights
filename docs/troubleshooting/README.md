# Troubleshooting index

Solved problems, one row per case. Search this table by area or symptom and
open only the matching file. Each case describes the code as of its fix commit;
check the code before applying an old fix.

To add a case, copy [_template.md](_template.md) to `TS-<next number>-<slug>.md`
and add a row here in the same change.

| ID | Date | Area | Symptom | Cause | Fix |
| --- | --- | --- | --- | --- | --- |
| [TS-001](TS-001-gpu-data-missing-flip-discard.md) | 2026-03-30 | GPU | No GPU scopes recorded with a `FLIP_DISCARD` swap chain | Ring buffer too short; query results not ready when read (inferred) | `b4dd9bb` |
| [TS-002](TS-002-ci-missing-std-include.md) | 2026-11-03 | Build | CI fails with C2039 (`'sort' is not a member of 'std'`) on code that builds locally | Missing standard include hidden by an older MSVC's transitive includes | `e115e73` |
| [TS-003](TS-003-ftk1011-long-build-path.md) | 2026-11-03 | Build | Configure or build fails with `FTK1011` in a deeply nested folder | Build paths exceed MSBuild's file tracker path limit | Workaround: short path |
| [TS-004](TS-004-stale-exe-under-temp.md) | 2026-11-05 | Build | Rebuild under `%TEMP%` leaves a stale exe; `MSB8029` warnings | Incremental build unreliable under `%TEMP%` (inferred) | Workaround: build outside `%TEMP%` |
