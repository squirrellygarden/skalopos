# Pillar 13 — skfs (on-disk filesystem)

> v2 deliverable. Not in v1. Documented now so the design is settled before block devices land.

## Goals

- A real on-disk filesystem for v2, replacing the in-memory initramfs as the place state lives.
- An improvement on FAT32 — Skalapos's user is comfortable with FAT internals and wants a "present-proof" successor, not a research filesystem.
- Hex-readable on disk by design: looking at an image in `xxd` is a first-class debugging method.
- Posix-style mode/owner stored even though Skalapos v2 doesn't enforce permissions yet (v3 can turn enforcement on without an FS rewrite).
- Metadata journaling planned as **v2.1**, a focused follow-on milestone after the base FS is solid.

## What FAT32 gets wrong (the targets)

1. No journaling — power loss corrupts the FAT.
2. Minimal per-file metadata; no owner/mode/extended attributes.
3. No checksums.
4. 4 GiB file size limit (32-bit field).
5. Cluster-chain scan is O(n) for random access.
6. 8.3 filenames with LFN bolted on; UTF-16 with weird case rules.
7. Free-cluster tracking IS the FAT (scan to allocate).
8. Directories are fixed arrays in regular files; resize = rewrite.
9. No symlinks, no real hardlinks.

## What FAT32 gets right (worth keeping)

- Hex-readable on disk.
- Conceptually flat — no tree-balancing, no recursive structures.
- Easy to implement correctness for in a kernel.

skfs keeps the *posture* (readable, flat, debuggable) and fixes the per-file metadata, naming, addressing, and free-space pieces. Journaling lands at v2.1.

## Contract — v2.0 (base FS, no journal yet)

### On-disk layout (4 KiB blocks)

```
Block  0:        Primary superblock
Block  1:        Backup superblock
Blocks 2..J-1:   Reserved for v2.1 journal region (sized at mkfs time; left zeroed in v2.0)
Blocks J..I-1:   Inode table (fixed region; size set at mkfs)
Blocks I..B-1:   Free-block bitmap
Blocks B..end:   Data blocks
```

Block size is **4 KiB**, period. Matches both target arches' page size. Matches universal disk sector groupings. Documented; not configurable.

### Superblock (block 0, with backup at block 1)

```
offset  size  field
  0     4     magic           = "SKFS" (ASCII, byte order: 0x53 0x4b 0x46 0x53)
  4     4     version_major   = 1     (1 = v2.0; bump for v2.1 journaling)
  8     4     version_minor   = 0
 12     4     block_size      = 4096
 16     8     total_blocks
 24     8     inode_table_start_block
 32     8     inode_table_block_count
 40     8     freemap_start_block
 48     8     freemap_block_count
 56     8     data_start_block
 64     8     journal_start_block      (set to 0 in v2.0; populated in v2.1)
 72     8     journal_block_count      (0 in v2.0)
 80     8     root_inode_no            (typically 1)
 88     8     free_block_count
 96     8     free_inode_count
104    16     uuid                     (random; identifies the FS instance)
120     8     mkfs_time_ns
128     8     last_mount_time_ns
136     8     last_unmount_time_ns
144     1     mount_state              (0=clean, 1=mounted-dirty)
145   367     reserved (must be 0)
```

Total: 512 bytes. The rest of block 0 (next 3584 bytes) is reserved.

Backup superblock (block 1) is byte-identical at mkfs time; the FS rewrites it on a clean unmount but never on a per-op basis.

### Inode (256 bytes)

```
offset  size  field
  0     2     type             0=free, 1=file, 2=dir, 3=symlink, 4=device-special, 5..=reserved
  2     2     mode             POSIX-style permission bits (stored; not enforced in v2)
  4     4     owner_uid        Stored as placeholder; ignored in v2. v3 may enforce.
  8     4     owner_gid        Same.
 12     2     nlinks
 14     2     flags            FS-internal flags (reserved 0 in v2)
 16     8     size_bytes       64-bit; file size or, for dirs, on-disk size of the directory blob
 24     8     blocks_used      Count of 4 KiB blocks the inode references
 32     8     atime_ns         ns since unix epoch
 40     8     mtime_ns
 48     8     ctime_ns
 56     8     btime_ns         birth/creation time
 64    96     direct[12]       Twelve 64-bit direct block pointers (covers files up to 48 KiB)
160     8     indirect         Pointer to a block that holds 512 block-pointers (extends to ~2 MiB)
168     8     double_indirect  (~1 GiB)
176     8     triple_indirect  (~512 GiB)
184     8     xattr_block      Pointer to a block holding extended attributes (NULL = none)
192    64     reserved         (must be 0; growable in future versions)
```

Total: 256 bytes. 16 inodes per 4 KiB block. The ext2/ext3 lineage in plain sight, intentionally — it's a known-good design.

**Why direct/indirect blocks, not extents?** Extents are nicer but add real implementation surface (tree balancing, splits, merges). The base v2.0 ships with indirect-block addressing for tractability. Extents are a possible v3 (call it v2.2 or v3.0) follow-on if file sizes or fragmentation make it worth it.

### Directory entries

A directory's "data blocks" hold a sequence of variable-length records:

```
offset  size  field
  0     8     inode_no
  8     2     rec_len          total record length, multiple of 8 bytes
 10     2     name_len         in bytes
 12     1     type             same enum as inode.type (cached here to avoid an inode read on listing)
 13    N     name              UTF-8, NOT null-terminated; padded with NUL to make rec_len a multiple of 8
```

- Minimum record length: 16 bytes (1-byte name, padded to 16).
- Entries are stored linearly; deletion replaces with a "tombstone" record (`inode_no = 0`) that the next allocation can reuse.
- Lookup: linear scan in v2.0. ~50ns per entry; fine up to a few hundred entries per dir. Hash-indexed dirs are a v3 nice-to-have.
- Maximum name length: 255 bytes (one byte's worth of length, like POSIX).
- Filenames are UTF-8 byte sequences. No normalization. No case-folding. Two names that differ in byte sequence are different names. Document this in the FS reference doc.

### Free-block bitmap

One bit per data block: 0 = free, 1 = allocated. Sized to cover the data region. Allocation is "scan from a cursor that the kernel keeps in memory" — first-fit, with rotor.

Mount-time consistency: the bitmap's set bits must equal the union of every allocated inode's block references. v2.0 checks this at mount only if mounted-dirty; v2.1 adds a fast journal-based check.

### Inode allocation

Inodes are numbered 1..N where N is `inode_table_block_count * 16`. Inode 0 means "no inode" (analog of NULL). Inode 1 is conventionally the root directory.

Free inodes are identified by `type == 0` in the inode table. Allocation: scan; rotor; first-fit. There's no separate inode bitmap — the inode table is small enough that the linear scan is cheap.

### Symlinks

- Short symlinks (target ≤ 60 bytes): target stored in the `direct[]` array of the inode, in-line. `blocks_used = 0`. No data blocks consumed.
- Long symlinks: target stored in a regular data block, addressed normally.

### Extended attributes (xattrs)

If `inode.xattr_block != 0`, that block holds:

```
offset  size  field
  0     4     magic = "XATR"
  4     4     entry_count
  8     —     entries (variable-length, name_len + value_len + name + value)
```

xattrs are user-namespaced (`user.foo`) or system-namespaced (`system.foo`). v2.0 stores them; v2 doesn't use them for anything. Useful for v3 features (immutable bits, custom flags, capability-style ACLs, ...).

### Mount state

On mount: read superblock, check `mount_state`. If 1 (mounted-dirty), v2.0 refuses to mount writable and prints a diagnostic recommending `mkfs` or `fsck` (v2 ships only a stub `fsck` — its diagnostic is "filesystem may be inconsistent"; v2.1's journal makes this clean). On clean mount, write `mount_state = 1` to both superblocks.

On unmount: flush all dirty blocks, write `last_unmount_time_ns`, set `mount_state = 0`, write both superblocks.

A FS that wasn't unmounted (machine killed mid-run) on next mount has `mount_state = 1`. In v2.1, this triggers journal replay.

## Contract — v2.1 (metadata journaling)

Treated as a focused follow-on milestone, not a v2.0 feature.

Approach: **write-ahead metadata journal, no data journaling.** Crash safety covers the filesystem's structural integrity; in-flight data writes can still be lost on power loss.

Journal layout:

```
Blocks J0..Jn:    Journal region (sized at mkfs, typically 16-64 MiB)
                  Treated as a circular log. Three pointers (in superblock):
                    journal_head_seq    (next write position)
                    journal_committed_seq (everything ≤ here is on disk)
                    journal_applied_seq (everything ≤ here is in-place)
```

Record types:
- `J_BEGIN_TXN{txn_id}`
- `J_WRITE{block_no, new_contents[4096]}`
- `J_COMMIT_TXN{txn_id, checksum}`

Every metadata operation is wrapped in a transaction:

```
journal_begin(txn)
   journal_write(txn, freemap_block_X, new_contents)
   journal_write(txn, inode_block_Y, new_contents)
   journal_write(txn, dir_data_block_Z, new_contents)
journal_commit(txn)        // waits for journal blocks to hit disk
journal_apply_async(txn)   // writes the actual blocks in place, lazily
```

On mount, if `mount_state == 1`:
1. Scan journal from `journal_applied_seq` forward.
2. For each `COMMIT`ed transaction, replay its writes to their actual block locations.
3. For uncommitted transactions, discard.
4. Set `journal_applied_seq = journal_head_seq`. Write a new clean superblock.

This is the ext3-style approach (writeback-mode journal) and is the simplest correct implementation. ~800 LOC on top of v2.0.

**v2.1 bumps `version_minor` to 1.** A v2.1 image is forward-compatible with v2.0 readers (they'll see `journal_start_block != 0` and refuse to mount safely without journal support — which is the *correct* behavior).

## Hex-readability — explicit design rule

This is not aesthetic, it's load-bearing for development:

- All multi-byte integers are **little-endian**.
- All on-disk structures align on 4- or 8-byte boundaries.
- All "this is structure X" markers use **ASCII magic** (`"SKFS"`, `"XATR"`, `"JRNL"`).
- Block size is exactly 4 KiB so block-numbered offsets map cleanly to hex dump positions (`offset = block_no << 12`).
- No bit-packing for the sake of saving bytes in metadata fields. If a field needs 16 bits today, give it 16 bits.
- Reserved fields are explicitly named `reserved` and must be zero.

Documented in `docs/reference/skfs-disk-format.md` (to be written when v2 work starts) with byte-perfect layouts and example hex dumps for each structure.

## VFS / driver integration

skfs registers as a filesystem type the same way initramfs and tmpfs do (see pillar 5):

```c
status_t skfs_init(void);   // called from kmain
// Inside: fs_register("skfs", &skfs_ops);
```

`fs_ops` provides: `mount(block_dev_h, target_dir_h, opts)`, `unmount`, plus the per-inode/per-file callbacks the VFS layer expects.

The block device is a `File` handle (per pillar 8's D2 model) of device-class `DEV_BLOCK`, opened from `/dev/<blockdev>`. skfs reads and writes through `file_read`/`file_write` (positional, 4 KiB-aligned) and the `dev_op(BLOCK_FLUSH)` op for flushes. No special block-IO path; the existing syscall ABI is enough.

## Why this over alternatives

- **Direct port of FAT32** — keeps every wart. Defeats the purpose.
- **exFAT or NTFS** — proprietary lineage, no clean spec, complex on-disk format. exFAT is technically usable but inherits enough FAT-isms to not be worth implementing from scratch.
- **ext2 / ext4** — the closest ancestor. We're essentially designing "ext2 with sane choices" — UTF-8 names, 64-bit timestamps in ns, xattr support from v1, hex-readable discipline. Compatibility with ext2 was considered and rejected — there's no win, and matching ext2 byte-for-byte means inheriting historical layout decisions we'd otherwise improve.
- **Full btrfs-style COW** — beautiful, multi-month project, way beyond v2 scope.
- **Journal first, base FS later** — wrong order. The base FS is what stores data; you can't journal a thing that doesn't exist. v2.0 ships the on-disk format and the read/write paths; v2.1 wraps the metadata writes in a journal.

## v2+ direction beyond v2.1

- **Hash-indexed directories.** For directories with many thousands of entries. Optional; v3 if dirs ever feel slow.
- **Extent-based addressing.** Replace direct/indirect with extent trees for files. Helps random access on large files. v3.
- **Per-block checksums.** A separate "checksum blocks" region; CRC32 of each data and metadata block. ~+500 LOC. Catches silent corruption. v3.
- **Snapshots.** Requires COW metadata, which means rewriting the inode model. v3+ if at all.
- **`fsck` proper.** v2 ships a stub; v2.1's journal makes most of the v2 problems moot. A real offline `fsck` is its own utility, host-side or guest-side.
- **Compression / encryption.** Not in the foreseeable plan.
- **Quotas.** Pair with the privilege story whenever that lands.
