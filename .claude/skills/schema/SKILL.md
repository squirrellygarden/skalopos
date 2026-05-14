---
name: schema
description: Add, remove, or modify a syscall or status code in the Skalapos ABI. Invoke with /schema [description of the change]. Edits schemas/syscalls.toml, runs codegen, and updates the relevant pillar doc.
disable-model-invocation: true
allowed-tools: Read Edit Bash
argument-hint: [describe the syscall or status code change]
---

You are making an ABI change to **Skalapos** — adding, removing, or modifying a syscall or status code.

`schemas/syscalls.toml` is the single source of truth for all syscall numbers and status codes. **Never hand-write a syscall number or status code in two places.** Codegen does the rest.

## Step-by-step process

### 1. Understand the change

Read the task description carefully:
- Is this a new syscall, a modified signature, a renamed syscall, or a removed syscall?
- Is this a new status code, a renamed code, or a removed code?
- Which pillar doc covers this subsystem?

### 2. Read the schema and the affected pillar

Read `schemas/syscalls.toml` in full before editing — understand the existing layout, naming conventions, and numbering.

Read the relevant pillar doc in `docs/pillars/` — specifically the **contract** section. If the change conflicts with the contract, stop and ask before proceeding.

### 3. Edit the schema

Edit `schemas/syscalls.toml` following the existing conventions:
- Syscall names: `snake_case`, verb-first (e.g., `vm_alloc`, `file_read`, `proc_spawn`)
- Status code names: `ALL_CAPS`, subsystem-prefixed (e.g., `VM_OUT_OF_MEMORY`, `FS_NOT_FOUND`)
- Don't reuse or skip numbers — append new ones at the end of their group, or renumber only if doing a deliberate cleanup
- Add a short comment for each new entry explaining its purpose

### 4. Run codegen

```
just codegen
```

Fix any codegen errors before continuing. Do not proceed with a broken schema.

### 5. Update the pillar doc

If the change is semantic (not just a rename or cosmetic fix), update the **contract** section of the relevant pillar doc to reflect the new ABI commitment. If the change is significant enough to affect the **pseudocode** section, update that too.

If the change adds a new capability that doesn't fit any existing pillar, stop and ask whether a new pillar doc or a proposal is needed.

### 6. Check for stale hand-written references

Search for any hand-written syscall numbers or status code strings that should now come from generated headers:

```
grep -r 'SYS_\|STATUS_' kernel/ userland/ arch/ --include='*.c' --include='*.h'
```

Flag anything that looks like it was hand-written rather than generated.

## What you must NOT do

- Do not edit generated files directly — they come from codegen and will be overwritten
- Do not assign a syscall number manually in a `.c` or `.h` file
- Do not remove a syscall without checking whether it is referenced in any pillar doc contract

## ABI change requested

$ARGUMENTS
