# M7-016 current GPU copy callers

2026-09-07. M7 remains0/16. This follows the
[published-scope residual profile](M7-016-published-scope-profile.md) and uses the
same E54C42E5... host/package09BAE976..., GPU18A33A99... and VCRUNTIMED5E4D9A3....
No game binary, wait, copy or cache-coherency behavior changed for this capture.

## Capture and verification

Private root: `private/evidence/M7-016/guest-gpu-copy-profile-20260906`.
A three-second99Hz userspace scout selects one thread through exact GPU function
ranges, at least two hits, stable birth/mappings and the SDK worker name. An
eight-second recording then captures that thread's twenty x86 registers and up
to4096 valid stack bytes per sample. Both stages fit inside the observed neutral
unpaused window. No kernel samples, inheritance or arbitrary process capture.

The pure parser passes556 retained known-answer records, eight positive cases
and48 rejection cases. Extracted controller tests cover the real header, four
header negatives, two selector known answers and six selector negatives. Testing
caught the existing perf DWARF/register warning before deployment; only that
exact warning is allowed. Actual raw register masks remain checked, and automatic
DWARF unwinding is explicitly untrusted. Leaf text is rendered with `perf script -G`.

Capture42890 ends normally: title147107, selected TID147143, no forced cleanup,
survivors or identity drift. The scout has607 samples and ten qualifying hits;
all505 stack samples have usable registers and2000–4096 valid bytes. Six commands,
four snapshots, twelve images, full seed and phase/frame ordering verify.
Scout recording is3.185703s; stack recording8.237632s. Stack recording starts
6.317506s after observed BEGIN and completes5.684678s before observed END in
controller time. These are not a clock bridge or exact counter containment.

All56 returned files /49,613,890bytes are verified and safely extracted. Primary
inspected five gameplay/unpaused/pause frames: the unpaused endpoints retain the
rear camera and HUD0mph; scene details vary. No moving-gameplay/audio or clean
FPS comparison is claimed.

- Controller: `DDE30423A910EEEF0C183CA291DE667A7F33BC054B0EAF4136D94416D3C29493`
- Stack parser: `AB1BF886CCDA82151B4C372FAE40F5B9A667548BB543CB3E9D920EB10DF86491`
- Archive: `06C6FB485D27B5010B346EF51304ED088D561E058242AEB62DFFF08CDEE40358`
- Inventory: `186B46A36612805A13A57D099CC60E446ABB455B0D2A03E6C66B695E55E81161`
- Return analysis: `FF73B072B114F601EA28D9BEE3EA4CB244A16F5F0234BA703A1171C9D565B76F`
- Return verification: `54983D0E34FF400EBA254A71E56C685CEEB44F9ABB3AC1E0B2B5961A558EF721`

## Exact caller and size findings

There are76 leaves in VCRUNTIME's `memmove` body. Sixty-nine fall in the previously
verified forward AVX instruction states; seven other states remain excluded.
The identical CRT body and unwind record establish the return slot without
searching arbitrary stack words. Each accepted return ends a decoded call in an
exact SDK function range, through thunk337BB0 and IAT5CEC28, whose import table
names `VCRUNTIME140.dll!memcpy`.

| Caller | Accepted observations | Represented size range |
|---|---:|---:|
| `IssueCopy_ReadbackResolvePath` |47|32KiB–5MiB|
| `VulkanSharedMemory::UploadRanges` |22|4KiB–444KiB|

All69 accepted observations yield sizes in25 bins. At these specific instruction
states, RAX retains the original destination and `R8 + (RCX - RAX)` remains the
total copy length; alignment adjustments and completed loop iterations preserve
the sum. Four transient states are excluded. Sample counts describe CPU residency,
not invocation counts or transferred-byte totals. File/map/import identity is
not live IAT or live-code attestation. Independent raw reconstruction and primary
reproduction agree on all caller groups and size bins.

- Stack raw data: `C3D9CFE15812A388B5A638B3D65ADAA369EE9E92D89EF0760BC62D3DB7AD6294`
- Caller analyzer: `D0ADAA36FF799CABECF71B995810B85C666B65EC4734D39D8A41C6799DB0EB37`
- Caller analysis: `7F2B94F8E124B888C991CE7392DBDC1569491AC194A60E7377FFD89897B43FF5`

## Rejected coherency change and next step

Do not pre-trigger guest-virtual write-watch callbacks before the resolve-readback
copy. The destination is `TranslatePhysical`, and the memory callback deliberately
excludes the raw physical view. Watch protection belongs to the A/C/E virtual
aliases. Thus these leaves do not show write-watch faults within the copy.

Resolve first marks the range valid/GPU-written. Pre-triggering a virtual alias
would clear those flags and fire CPU invalidation, potentially forcing upload of
the previous fast-readback slot. That changes authoritative data; it does not
prepay this copy's fault handling. The proposal was rejected before implementation.
Cached readback memory is already preferred by the current allocation helper.

The [guest-free copy comparison](M7-016-resolve-copy-witness.md) now passes728
correctness cases and224 timing rows on native Windows and exact Proton. It
rejects a global CRT replacement: benefits depend on size/cache reuse, and
ordinary owned-memory buffers do not reproduce actual Vulkan allocations.
No copy suppression,
new watch policy, wait bypass, production promotion, version bump, task closure,
commit or push has occurred.
