# skfs on-disk format (v2.0 base + v2.1 journaling)

Placeholder reference document. Full byte-perfect layouts and hex-dump examples will live here once skfs implementation begins (v2 work). The pillar doc [`../pillars/13-skfs.md`](../pillars/13-skfs.md) has the design and rationale; this document is the *binary spec* a reader/writer reaches for.

## Sections (to be filled in)

1. Overall layout (block-by-block)
2. Superblock (block 0) — byte-perfect field layout
3. Backup superblock (block 1) — same
4. Journal region (v2.1) — record format, sequencing rules
5. Inode table — inode at byte-perfect offset
6. Directory entries — variable-length record format
7. Free-block bitmap
8. Extended attribute block
9. mkfs invariants (what a freshly formatted FS looks like)
10. Mount-state state machine (clean / dirty / journal-replay)
11. Worked hex dump examples (an empty FS, a FS with one file, a FS mid-journal-replay)
12. Compatibility matrix between v2.0 and v2.1

## Rules of thumb when filling this in

- Every byte of every structure is documented or marked `reserved (must be 0)`.
- Endianness, alignment, magic numbers stated up front.
- Each structure has at least one **hex dump example** with annotations: a real reader should be able to write a parser from this document alone.
