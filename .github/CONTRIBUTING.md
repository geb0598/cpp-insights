# Contributing

## Code style

Match the surrounding code; it is consistent and hand-aligned.

- 4-space indent, no tabs, lines up to ~110 columns, `#pragma once`.
- `PascalCase` types and methods, `snake_case` locals and parameters,
  `snake_case_` private members, `SCREAMING_SNAKE_CASE` constants.
- Consecutive declarations, assignments, and initializer lists are aligned into
  columns by hand. Preserve the alignment; do not run a formatter over it.
- Logical blocks are separated by banners:

  ```cpp
  // -------------------------------------------------
  // ClassName
  // -------------------------------------------------
  ```

- Singletons delete copy and move constructors and assignment operators.

## Commits

Conventional Commits: `<type>: <imperative summary>`, at most 72 characters.

| Type | Use for |
| --- | --- |
| `feat` | New behavior |
| `fix` | Bug fix |
| `refactor` | Code change with no behavior change |
| `test` | Adding or changing tests only |
| `build` | CMake, packaging, dependencies |
| `ci` | GitHub Actions workflows |
| `docs` | Documentation only |
| `chore` | Anything else (tooling, housekeeping) |

- If the diff explains itself, write the subject line only.
- Add a body only for what the diff cannot show: why the change was needed, a
  rejected alternative, a non-obvious consequence, or a linked issue
  (`Fixes #12`). Wrap at 72 columns.

## Documentation

Prefer the code over prose about the code. Each kind of knowledge has one home:

| Knowledge | Home |
| --- | --- |
| Commands, cross-cutting gotchas | `AGENTS.md`; keep it short and update it in the same change |
| A non-obvious constraint on specific code | A comment at that code |
| A solved problem whose cause was hard to find | `docs/troubleshooting/`, one file per case plus an index row |
| Open bugs and planned work | GitHub Issues |
| A multi-step procedure that recurs | A skill in `.agents/skills/<name>/SKILL.md` |
| User-facing usage | `README.md` and `README.ko.md`, changed together |

Do not add documents that describe how the current code is structured.

## Issues

Use the forms in `.github/ISSUE_TEMPLATE/`: **Bug** for defects, **Improvement**
for planned work. One problem per issue, with the files involved and acceptance
criteria a reviewer can check.

When fixing an issue required real diagnosis, add a troubleshooting case that
links the issue, and link the case from the issue.

Agents draft issues in the form's fields and show them to the maintainer before
creating, editing, or closing anything.
