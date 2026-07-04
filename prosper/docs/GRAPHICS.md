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

### The current deterministic terminal fault (verified 2026-07-03, corrects an earlier mis-diagnosis)

`boot_trace` ends every run at the same place. The last log line is Unity's own:

```
todo: void GfxDevicePS5SharedData::CreateWorkload()
=== RUN ENDED: SIGSEGV at addr=0x30 rip=eboot+0x3b5ea6  rdi=0x0 ===
```

So this is a **graphics-path fault inside `GfxDevicePS5SharedData::CreateWorkload()`**, *not* a
boot-time C++/locale-construction mystery (an earlier working note wrongly framed it as a "rune
facet never set during static init" — there is **no** eboot `init_array` at all: `init_array_va=0`,
and the game initializes its C++ via IL2CPP + guarded function-local statics, so no static-init is
being skipped). Chain, all verified by disassembly under gdb (`break run_entry` first):

- `eboot+0x3b5ea0` is a `std::ctype`-style classify routine: `movzwl 0x2e(%rdi,%rcx,2),%esi` /
  `mov (%rdi),%rax; movzwl (%rax,%rcx,2)` — `rdi` is the classification **table** pointer, indexed
  by char code. It faults because `rdi == 0`.
- Its caller `eboot+0x3afb90` loads that table as **`[facet+0x8]`** (`mov 0x8(%r14),%rdi` at
  `+0x3afbc8`, where `r14` = the facet). So the offending facet's `+0x8` table field is **null**.
- The facet pointer is `obj+0x38 + facet_index*0x70` (extractor `eboot+0x3b0210` returns
  `rdi + esi*0x70`; caller `eboot+0x3a7b60` does `rdi += 0x38` first). I.e. `obj+0x38` is an inline
  array of 0x70-byte facets; this is a `std::locale` built in the GfxDevice path.
- **`_Getpctype` is NOT the culprit — it already works.** It is implemented (`h_getpctype` →
  `build_ctype`, returns a real 257-entry table) and *does* bind and get called (confirmed: a
  breakpoint on `prosper::h_getpctype` fires, called from `eboot+0x82e898`). So the classic-locale
  ctype table exists; this *particular* locale/facet built in `CreateWorkload` just never gets its
  `+0x8` table populated.

**Refined by the `PROSPER_FAULTMEM` probe (2026-07-04, deterministic):** at the fault, `r14` (the
facet, `obj+0x38`) points to a **fully-zeroed object** — `[+0]=[+8]=[+0x10]=[+0x18]=0`. So it is not
merely the ctype table field that is null; **the locale's entire inline facet array is zero.** The
`std::locale` (`obj` at `r14-0x38`) was allocated + zeroed but **no facets were ever installed** (the
facet-copy from the classic locale never ran, or ran on a different object). `rbp[+8]=0x4014ddc1a`
confirms the `CreateWorkload` caller frame; `obj` is loaded from `[rbp-0x198]` at `eboot+0x14ddc02`.

**Open question for the next deep session:** why this `std::locale`'s facet array is left all-zero —
i.e. where the facet-install/copy-from-classic step is skipped. Likely the classic-locale singleton
(`std::locale::classic()`) itself never populated its facets (a guarded static init that bailed on a
stubbed dependency), so every derived locale copies zeros. Next: find the classic-locale facet array
and check whether it too is zero (if so, fix its init); trace what builds `obj` at `[rbp-0x198]` in
`eboot+0x14ddc00`'s fn. **Tooling:** run `PROSPER_FAULTMEM=1 ./build-linux/boot_trace <dump>` — it
dumps every GP register and 4 qwords of guest memory at each pointer-looking one, *at fault time on
the stopped faulting thread* (reliable; live gdb breakpoints race in this multithreaded, signal-
scheduled guest and give garbage register readouts).

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
