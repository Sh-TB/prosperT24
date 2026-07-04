# Graphics & Audio bring-up (the M4/M5 frontier)

Status as of the boot reaching multithreaded graphics/audio initialization. The game now runs its
**entire non-graphics runtime** — IL2CPP init, C# startup, the runtime main flow, splash/message
dialog, the flip/render loop scaffolding — and enters GPU + audio setup. This doc is the blueprint
for turning that into actual rendered frames.

## How far the boot gets

```
loader → CRT → C++ ctors → il2cpp_init (GC, metadata, type system) → runtime startup
  → sceSystemServiceHideSplashScreen → sceMsgDialog (auto-dismissed)
  → GRAPHICS init: libSceAgc (GPU command build) + libSceAgcDriver (submission)
  → libSceVideoOut (display / flip / vsync event queues)
  → AUDIO init: libSceAmpr
  → [BLOCKED] multithreaded null-object derefs in graphics/audio worker threads
```

The block is the **headless limit**: our placeholder graphics/audio objects are zeroed, so worker
threads eventually read a null sub-object pointer out of them and dereference it (e.g.
`eboot+0x3b5ea6` `[null+0x30]`, `eboot+0x149c99c` `[null+0x18]`). Zeroed placeholders no longer
suffice — the game needs **real object graphs**, i.e. the actual graphics/audio subsystems.

### The terminal fault is unimplemented libSceAgc, NOT a C++ locale bug (verified 2026-07-04)

**This corrects two earlier mis-diagnoses in this file's history** (first "rune facet never set during
static init", then "std::ctype locale facet array left zero"). Both were wrong: the table-lookup
*instruction shape* at `eboot+0x3b5ea0` (`movzwl 0x2e(%rdi,%rcx,2)`) merely *resembles* `std::ctype`
classification. The object it reads actually comes from an **unimplemented libSceAgc initializer**.

The last log line before the crash is Unity's own `todo: void GfxDevicePS5SharedData::CreateWorkload()`
— i.e. we are inside Unity's PS5 GPU-device setup. The crash **site varies across runs** (multithreaded
graphics workers): `eboot+0x3b5ea6` (`addr=0x30`), `eboot+0x149c99c` (`addr=0x18`), … — all null-field
derefs of zeroed graphics objects, not one deterministic path.

Verified chain for the `CreateWorkload` object (gdb, `break run_entry` first; `pltm` + the unimpl log):

- In `eboot+0x14dd*`'s fn: `obj = r14+0x48` (`lea 0x48(%r14),%r12` @`+0x14dda75`), then immediately
  `call 0x4003ae7d0(obj)` — the object's initializer.
- `0x4003ae7d0` is a **PLT stub**: `jmp *[0x401d95858]`. At runtime that GOT slot holds `0x600004180`,
  which is one of **our unimplemented-import stubs** (`mov $idx,%edi; movabs $prosper_on_unimpl; jmp`).
- `pltm eboot.bin known_names.txt 1d95858` → NID **`+kSrjIVxKFE`**, and the boot log shows
  `unimplemented: libSceAgc::+kSrjIVxKFE -> returning 0`. So `obj`'s initializer is an **unimplemented
  libSceAgc function** that does nothing.
- The caller then passes that *same* `obj` (`r12`, stored at `[rbp-0x198]`) to `0x4003a7b60`, which
  walks `obj+0x38` and reads `[obj+0x40]` as a pointer → it's null (never set by the stubbed init) →
  SIGSEGV. `PROSPER_FAULTMEM` confirms the whole `obj` region is zero at the fault.
- `_Getpctype` works and is called, but only for an unrelated inline `isspace`-style loop
  (`eboot+0x82e893`, `test $0x144`) — it is NOT part of this path.

**Conclusion:** the blocker is the **libSceAgc GPU object graph**. `CreateWorkload` (and sibling
graphics init) call ~24 libSceAgc functions — `23LRUSvYu1M`, `BfBDZGbti7A`, `+kSrjIVxKFE`,
`H7uZqCoNuWk`, `vRoArM9zaIk`, … (full list via `boot_trace`) — **all unimplemented, all returning 0**,
so the GPU objects they should build stay null and the graphics workers deref null. This is squarely
the M4/M5 libSceAgc→Vulkan work below; there is no locale/libc gap to fix. Faking these objects with
plausible-looking fields would be "limping to graphics" (violates correctness-first) — they need the
real AGC object model, reverse-engineered from the call args (`PROSPER_GFXLOG`) + AGC semantics.

**Tooling:** `PROSPER_FAULTMEM=1 ./build-linux/boot_trace <dump>` dumps every GP register + 4 qwords of
guest memory at each pointer-looking one, at fault time on the stopped thread (reliable; live gdb
breakpoints race in this multithreaded, signal-scheduled guest). `pltm eboot.bin known_names.txt
<got_off>` maps a GOT slot to its import NID/name.

**AGC call tracing (RE bootstrap for M4).** All 28 libSceAgc/AgcDriver NIDs the game calls are routed
through per-NID logging thunks (`glog_thunk<I>` in `hle_graphics.cpp`; behaviour unchanged — still
returns 0). `PROSPER_GFXLOG=1 ./build-linux/boot_trace <dump>` now emits a **self-describing** line per
call: `libSceAgc::<NID>  from eboot+0x<callsite>  a0..a5`. ~148 AGC calls fire before the fault.
Call-frequency profile of one run (NID ×count — the hot ones are the AGC command/descriptor ops to
understand first; args' high addresses are heap/GPU-VA that vary per run, but NIDs + callsites are
stable):
- `f3dg2CSgRKY` ×36 — hottest; a per-op/per-command call.
- `d-6uF9sZDIU` ×25, `ZvwO9euwYzc` ×25 — next hottest, paired.
- `TRO721eVt4g` ×5 — `CreateWorkload` per-object init (`eboot+0x14e6661`, `a0=a3=obj, a4=obj-0x48`);
  `obj` is the very object whose null field later faults, so this call (or `+kSrjIVxKFE` ×3, the
  initializer at `eboot+0x3ae7d0`) is what should populate it.
- device/context level (once each): `23LRUSvYu1M`, `BfBDZGbti7A`, `XlNp7jzGiPo`, `MM4IZSEYytQ`.
Next: map each hot NID + arg pattern to the AGC API to build the real object model, then implement the
initializers to construct valid GPU objects (correctness-first — no plausible-looking fakes).

## What's already in place (headless bring-up, correctness-first)

- **Unified GPU memory (lazy)** — `exec_image_linux.cpp` fault handler backs any unmapped page in
  the GPU-VA window `[4 GiB, 64 GiB)` with a real zeroed page on demand and retries. This models the
  PS5's unified CPU/GPU memory (GPU VAs are real RAM). Low-address null derefs stay fatal. It got the
  boot past the format-table fault at GPU VA `0x100000000` into audio init. **Contents are zero until
  the driver layer is real** — a documented placeholder, not faked output.
- **libSceVideoOut** (`hle_graphics.cpp`) — `Open`→handle, `SubmitFlip` increments a flip counter,
  `GetFlipStatus` reports it (correct 0x40 struct — do NOT over-write, it smashes the guest canary),
  `IsFlipPending`→false, event/flip machinery no-op. Simulated flip completion so the render loop
  advances.
- **libSceAgc** getters that return dereferenced objects → stable zeroed singletons.
- **event queues** (`hle_kernel_time.cpp`) — valid queue objects; `WaitEqueue` yields + reports no
  events.
- **Diagnostics** — `PROSPER_GFXLOG` logs AgcDriver call args.

## Build environment

- `libvulkan.so.1` (1.3.275) **is present**; Vulkan **headers are not** (`apt install libvulkan-dev`
  or vendor `vulkan/`). No SDL2/X11 → plan **headless/offscreen Vulkan** first (WSL has no display).
- Graphics libs are sparsely documented; most NIDs don't resolve to names — reverse-engineer from
  call args (`PROSPER_GFXLOG`) and `build-linux/tools/pltm` (maps a module GOT offset → NID).

## Recommended implementation order

1. **Real unified memory.** Make GPU allocations CPU/GPU-VA *aliased*: when the guest maps direct
   memory, back it so the GPU VA it later uses equals a valid CPU address (single physical page seen
   at both). This replaces the lazy zeroed-page placeholder with real, coherent memory — the
   prerequisite for everything else. Trace the libSceAgc memory-map calls to learn the GPU-VA scheme.
2. **libSceVideoOut swapchain.** Real window/offscreen images + a Vulkan swapchain (or headless
   render target); `SubmitFlip` presents the current buffer.
3. **libSceAgc → Vulkan.** Decode the AGC command buffers (draw/dispatch/state) and translate to
   Vulkan command buffers. This is the largest piece.
4. **RDNA2 shader recompiler.** Translate the game's GCN/RDNA2 shader ISA (or the AGC shader blobs)
   to SPIR-V. The other large piece.
5. **libSceAmpr audio.** Mix/output via a host audio backend (or a null sink first).

## Testing

Everything above must stay behind programmatic checks (agentic-first). Current suite (7 tests):
module parse, NID hashing, trap, boot (asserts the GC stop-the-world runs), setjmp, HLE registration,
SceKernelStat layout. Add: unified-memory aliasing test, AGC command-decode unit tests, a boot check
that asserts the first `SubmitFlip`.
