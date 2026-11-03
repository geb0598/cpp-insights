# Troubleshooting index

Solved problems, one row per case. Search this table by area or symptom and
open only the matching file. Each case describes the code as of its fix commit;
check the code before applying an old fix.

To add a case, copy [_template.md](_template.md) to `TS-<next number>-<slug>.md`
and add a row here in the same change.

| ID | Date | Area | Symptom | Cause | Fix |
| --- | --- | --- | --- | --- | --- |
| [TS-001](TS-001-gpu-data-missing-flip-discard.md) | 2026-03-30 | GPU | No GPU scopes recorded with a `FLIP_DISCARD` swap chain | Ring buffer too short; query results not ready when read (inferred) | `b4dd9bb` |
