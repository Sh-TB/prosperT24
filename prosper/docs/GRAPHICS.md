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
