---
name: docs
description: Write or update Skalapos documentation — pillar docs, overview, or implementation guide. Invoke with /docs [what to document].
disable-model-invocation: true
allowed-tools: Read Write Edit Bash
argument-hint: [pillar name, subsystem, or doc to update]
---

You are writing or updating documentation for **Skalapos**, a toy POSIX-evolution OS in freestanding C.

## Before writing anything

1. Read the existing doc you're updating, if one exists.
2. If writing or updating a pillar doc, read `docs/overview.md` and any related pillar docs that this one cross-references.
3. If the doc describes an implemented subsystem, read the relevant source files to ensure the doc matches the actual implementation — not the other way around.
4. If updating `docs/implementation.md`, check that phase ordering and prerequisites still make sense against the current state of the code.

## Document types and their required structure

### Pillar doc (`docs/pillars/NN-name.md`)

Every pillar doc must contain exactly these sections in this order:

```
# NN. Title

One-paragraph summary of the pillar's purpose.

## Goals

Bullet list of what this pillar achieves.

## Contract

The precise invariants this pillar guarantees. Written as obligations on the kernel and/or userland. These are ABI-level commitments.

## Pseudocode

Key operations shown in C-like pseudocode. Enough to implement from; not a tutorial.

## Rationale

Why this design was chosen over the obvious alternative. Reference POSIX pain points where relevant.

## v2+ direction

What changes or extends in v2 and beyond. Keep it brief — this is a pointer, not a spec.
```

Do not add sections; do not remove sections. The structure is locked.

### Proposal doc (`docs/proposals/name.md`)

Use the `/proposal` skill for new proposals. Proposals live in `docs/proposals/` and have their own structure.

### Overview doc (`docs/overview.md`)

Keep it to a five-minute mental model. Don't let it grow into a spec.

### Implementation guide (`docs/implementation.md`)

Phases 0–4. Each phase lists: what to build, what it depends on, and roughly how long it takes. Don't describe implementation details — that's what pillar docs and code comments are for.

## Writing style

- Technical prose. No marketing language, no filler, no "note that" or "it is important to".
- Short sentences. Active voice.
- Code blocks for pseudocode and examples. C syntax unless otherwise needed.
- One-line `///` comments in pseudocode for non-obvious invariants.
- Cross-reference other pillars by number and name: "See pillar 3 (Errors)."
- Do not duplicate information that lives in a pillar doc — link to it instead.

## Consistency check

If you're updating a pillar doc, verify:
- The **contract** section matches what the current implementation actually enforces (if implementation exists)
- Cross-references to other pillars are still accurate
- The **v2+ direction** section doesn't describe something that was already implemented

## What to document

$ARGUMENTS
