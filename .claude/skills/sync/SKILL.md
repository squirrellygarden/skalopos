---
name: sync
description: After a design change, find everything in the repo that is now inconsistent with it and propose or apply updates. Invoke with /sync to inspect staged/recent changes, or /sync [ref] to diff against a specific commit.
disable-model-invocation: true
allowed-tools: Read Bash Edit
argument-hint: [git ref, file, or leave blank for HEAD diff]
---

You are propagating a design change through the **Skalapos** repository — finding everything that became stale or inconsistent as a result of the change and bringing it up to date.

## Step 1 — Get the diff

If an argument looks like a git ref (commit hash, branch name, tag), diff against it:
```
git diff $ARGUMENTS
```

If an argument looks like a file path, show recent changes to that file:
```
git log --oneline -5 -- $ARGUMENTS
git diff HEAD~1 -- $ARGUMENTS
```

If no argument is given, use staged + unstaged changes:
```
git diff HEAD
```

Read the full diff carefully before doing anything else.

## Step 2 — Identify what changed

Classify each change in the diff. Common types:

- **Concept or name rename** — a syscall, struct, type, flag, or function was renamed
- **Signature change** — a syscall's arguments or return type changed
- **Contract change** — a pillar's invariant or behavior was updated
- **New syscall or status code** — something was added to `schemas/syscalls.toml`
- **Removed syscall or status code** — something was deprecated or deleted
- **Doc-only change** — a pillar doc or overview was rewritten; may imply the schema or code is now stale
- **Schema-only change** — `schemas/syscalls.toml` was edited; docs and generated files may be stale

For each changed item, write down: *what changed*, and *what could reference it*.

## Step 3 — Search for stale references

For each changed item, search the repo for references that may need updating. Use targeted greps — don't just scan everything blindly.

Examples:
```bash
# renamed syscall
grep -r 'old_name' --include='*.c' --include='*.h' --include='*.md' --include='*.toml' .

# changed struct field
grep -r 'field_name' --include='*.c' --include='*.h' .

# pillar cross-references
grep -r 'pillar 5\|05-filesystem\|pillar 6\|06-io' docs/ .
```

Places to check for each change type:

| Changed in | Check these for staleness |
|---|---|
| `schemas/syscalls.toml` | `docs/pillars/` (contract section), generated files (run `just codegen`), any `.c`/`.h` hand-referencing the name |
| A pillar doc contract | `schemas/syscalls.toml` (does the schema match?), other pillars that cross-reference this one, `docs/overview.md`, `docs/implementation.md` |
| A pillar doc name/number | All other pillar docs that link to it, `docs/overview.md` table, `CLAUDE.md` anti-patterns list, `schemas/syscalls.toml` `pillar =` fields |
| A C type or struct name | All `.c` / `.h` files using it, generated headers, pillar pseudocode |
| `docs/overview.md` | The relevant pillar doc(s), `README.md` summary table |
| `README.md` | `docs/overview.md` |

## Step 4 — Report findings before changing anything

Produce a clear list of stale locations found:

```
STALE: docs/pillars/06-io.md — contract section still says "offset=-1" but schema now uses "offset=SKALOPS_SEEK_CUR"
STALE: docs/overview.md — table row for pillar 6 references old "O_NONBLOCK" wording
GENERATED: run `just codegen` — syscall numbers_generated.h will be out of date
POSSIBLY STALE: kernel/io/file.c:42 — references `file_read` signature; check manually if arg order changed
```

Distinguish:
- **Definite** — the old string is literally present and needs changing
- **Generated** — requires running `just codegen`, not a manual edit
- **Possibly stale** — found a reference; needs human judgment to confirm

## Step 5 — Apply updates, with care

**Apply automatically** (safe mechanical changes):
- Doc cross-references where the old name/number appears verbatim and the new name is unambiguous
- `pillar =` fields in `schemas/syscalls.toml` if a pillar file was renamed
- `README.md` table entries that mirror a pillar doc change exactly

**Propose but don't apply** (requires judgment):
- Contract section rewrites in pillar docs — show the old text and your proposed replacement, ask for approval
- Any change to `schemas/syscalls.toml` entries — ABI changes need explicit confirmation
- Changes to `docs/implementation.md` phase structure
- Changes to `CLAUDE.md`

**Do not touch**:
- Generated files (`.c`/`.h` files named `*_generated.*`) — run `just codegen` instead
- Source code (`.c`/`.h` implementation files) — out of scope for this skill; flag them for `/dev`

After applying safe changes, list what you changed and what still needs human action.

## What was changed

$ARGUMENTS
