---
name: proposal
description: Draft a design proposal for a new Skalapos decision that doesn't fit an existing pillar, or that would change a locked-in pillar. Invoke with /proposal [topic].
disable-model-invocation: true
allowed-tools: Read Write Bash
argument-hint: [topic or design question]
---

You are drafting a design proposal for **Skalapos**, a toy POSIX-evolution OS in freestanding C.

Proposals live in `docs/proposals/` and are the right vehicle when:
- A task requires a design decision that isn't covered by any existing pillar
- A task seems to conflict with a locked pillar and the conflict needs to be resolved before code is written
- The user wants to explore an alternative to a locked decision before committing

Proposals are *inputs to a decision*, not decisions themselves. Write to help the user make a good choice, not to advocate for one answer.

## Before drafting

1. Read `docs/overview.md` to understand the project's core goals.
2. Read all potentially relevant pillar docs in `docs/pillars/`. Make sure you understand what is already locked and why.
3. Check `docs/proposals/` for any existing proposals on the same topic — don't duplicate.
4. Read the task description carefully. What is the actual decision to be made?

## Proposal file

Create `docs/proposals/<kebab-case-topic>.md` with the following structure:

```markdown
# Proposal: [Title]

**Status:** Draft
**Date:** [today's date]
**Affects pillars:** [list by number and name, or "none — new pillar"]

## Problem

One paragraph: what specific problem or gap makes this proposal necessary? Be concrete — reference the task or code that surfaced it.

## Constraints

What must the solution satisfy? Include: compatibility with locked pillars, v1 scope (single CPU, no persistent storage, no async I/O), and any hard requirements from the affected pillar contracts.

## Options considered

### Option A: [Name]

Brief description. Pseudocode if useful.

**Pros:**
- ...

**Cons:**
- ...

### Option B: [Name]

...

[Add more options as needed. Two to four is typical.]

## Recommendation

Which option you'd pick and the one-sentence reason. If you genuinely cannot recommend one (e.g., needs more information), say so and say what information is needed.

## Open questions

Bullet list of things that need to be resolved before this proposal can be accepted. If none, omit this section.
```

## Writing style

- Neutral, technical prose. Present tradeoffs accurately, including tradeoffs against the option you recommend.
- Don't advocate. The user makes the decision; your job is to surface the tradeoffs clearly.
- Keep each option description short — if pseudocode is needed to understand the tradeoff, include it, but don't write full implementations.
- If an option conflicts with a locked pillar, say so explicitly and explain whether the conflict is fundamental or resolvable.

## After drafting

Tell the user:
1. Where the proposal file was written
2. Which option you recommend and why (one sentence)
3. What needs to happen next (e.g., "review the options and tell me which direction to take, then I can proceed with /dev or /schema")

Do not start implementing until the user accepts a direction from the proposal.

## Proposal topic

$ARGUMENTS
