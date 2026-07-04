# AGC graphics — implementation plan (the M4/M5 frontier)

State as of 2026-07-04. The full non-graphics runtime is solid (real libc, IL2CPP, threads, memory,
services) and boots cleanly to the **graphics/AGC frontier**. This doc is the actionable plan for the
graphics phase, consolidating what we now know. It's a large effort — scoped honestly below.

## Where the boot stops

`GfxDevicePS5SharedData::CreateWorkload()` builds GPU command buffers and creates shaders via libSceAgc,
then faults reading a null field of a command-buffer object (`eboot+0x3b5ea6` / `0x149c99c`, varies by
thread). libSceAgc is a PS5 system library **not in the dump**, so we HLE it; our stubs return 0, so the
command-buffer/shader objects the game builds are never populated → null-deref.

## What we know (reverse-engineered)

**The AGC call surface is identified** (via shadPS4's `aerolib.inl` NID map — see `AGC_TRACE.md`). The
game's per-boot AGC usage:
- `sceAgcCreateShader` ×36 — create shader objects from RDNA2 blobs.
- `sceAgcDcbSetCxRegistersIndirect` / `sceAgcSetCxRegIndirectPatchAddRegisters` ×25 — set context regs.
- `sceAgcDcbResetQueue`, `PushMarker`/`PopMarker`, `EventWrite`, `WriteData`, `WaitRegMem`,
  `WaitUntilSafeForRendering`, `SetUcRegistersIndirect` — build the Draw Command Buffer (DCB).
- `sceAgcCbReleaseMem` / `DcbAcquireMem` / EOP-action patches — fences / memory sync.
- `sceAgcDriverSetTFRing` / `SetHsOffchipParam` — tessellation rings; `DriverGetEqContextId` /
  `AddEqEvent` — event-queue wiring; `sceAgcSuspendPoint` — TRC suspend point (the spin-loop message).

**The command-buffer object layout (partially reversed)** in `CreateWorkload`'s frame:
- `r14` = a **command-buffer wrapper** (Unity's): `+0x58` = buffer base ptr, `+0x60` = current write
  ptr, `+0x68` = a **virtual flush/grow fn**, `+0x78` = dword count. Append pattern:
  `if ((cur-base)>>2 >= count) flush()` then write.
- `obj = r14+0x48` = the AGC Dcb handle (passed to `sceAgcDcbPushMarker(obj)`).
- `[obj+0x40]` (= `[r14+0x88]`) = a **null table pointer** — read by a per-element lookup
  (`[table+0x2e+i*2]`, 16-bit entries; `[table][i*2]`). This is the field that faults. **Open: whether
  it's an AGC register/format table or a Unity-side table, and which call should populate it.**

## The gating unknown

The **exact AGC struct layouts** (`sce::Agc::Core::Dcb`, `Shader`, `StateBuffer`, register/descriptor
formats). shadPS4 does **not** have these (it's GNM/PS4, not AGC/PS5 — only the NID names transfer).
Two ways to get them, fastest first:
1. **AGC SDK headers** (`libSceAgc` public headers define these structs exactly). If available, this
   collapses weeks of RE into reading headers. ← biggest accelerator.
2. **Reverse from usage** — disassemble the game's field accesses per object. Slow but done piecemeal
   already (the command-buffer wrapper above). Doesn't need anything external.

## shadPS4 reusability (from the survey — GPL-2.0-or-later, OK to reference/open-source)

| Reuse near-as-is | Rewrite for PS5/RDNA2 |
|---|---|
| Generic Vulkan engine (instance/scheduler/swapchain/presenter/pipeline-cache/pools) | RDNA2 shader-ISA decoder (`shader_recompiler/frontend`) — the tentpole |
| Shader IR + SPIR-V backend (`shader_recompiler/ir`, `backend/spirv`) | RDNA2 register model + PM4/AGC-DCB opcode tables (`amdgpu/regs*`, `pm4_*`, `liverpool.cpp`) |
| Unified-memory + page-fault/dirty tracking + buffer/texture caches | V#/T#/S# descriptor bit layouts (`amdgpu/resource.h` — GCN → RDNA2), tiling (`tiling.cpp`) |
| CP→rasterizer→pipeline dataflow (architecture) | GNM HLE → AGC HLE (the `sceAgc*` surface; shadPS4's 325 stubs = the backlog) |

## Phased plan

1. **AGC object model** (gating). Get the Dcb/Shader/StateBuffer layouts (SDK headers ▶ or RE). Deliver:
   `sceAgcDcbResetQueue` initializes a real DCB over a game-provided buffer; `sceAgcCreateShader`
   allocates+parses a shader object; the `Dcb*` append fns write PM4 into the DCB. This clears the
   `CreateWorkload` fault and lets the game build command buffers **headlessly** (no GPU yet — real
   command-buffer semantics, not faked pixels).
2. **Submit → PM4 capture.** Implement the submit path (`sceAgcDriverAgrSubmitMultiDcbs`); decode the
   DCB PM4 stream (adapt shadPS4 `liverpool.cpp` + `pm4_*`, re-tabled for RDNA2). Verify by dumping a
   decoded command stream (programmatic golden check). Game frame-loop advances headlessly.
3. **Vulkan backend.** Stand up shadPS4's generic Vulkan engine (near drop-in); translate decoded PM4
   state → Vulkan (adapt `liverpool_to_vk` + `vk_rasterizer` for RDNA2 registers). Offscreen render
   target first (WSL has no display).
4. **RDNA2 shader recompiler.** Reuse shadPS4's IR + SPIR-V backend + structurizer; write a new
   gfx10.3/wave32 decoder frontend (cross-ref LLVM AMDGPU / Mesa RADV). The largest single piece.
   **CONFIRMED (2026-07-04):** `sceAgcCreateShader(out=a0, header=a1, code=a2, ?, size=a4≈0x100, a5)`.
   - `a1` header begins with ASCII magic **`"1234"`** + size fields (`0x18`, `0x108`, `0xa8`).
   - `a2` is raw **RDNA2 gfx1030 ISA** — verified by disassembling a captured blob (`build-linux/
     shader0.bin`, via `PROSPER_AGCSHADER=1`) with **`llvm-mc-18 --disassemble -triple=amdgcn
     -mcpu=gfx1030`**, which decodes it cleanly to a valid fullscreen vertex/primitive shader:
     `s_sendmsg MSG_GS_ALLOC_REQ; exp prim …; v_cvt_f32_i32 …; exp pos0 … done; s_endpgm`.
   - **Implication:** LLVM's AMDGPU backend already fully understands our shader ISA — it's a working
     disassembler/reference and a candidate for the decode frontend (vs. hand-writing a gfx1030
     decoder). Path: RDNA2 gfx1030 → (LLVM MC decode or shadPS4-style decoder) → IR → SPIR-V (reuse
     shadPS4's backend). `llvm-mc-18` + `libvulkan-dev` are now installed in the WSL env.
5. **Present.** `sceVideoOut` swapchain (already stubbed) → present the rendered target; framebuffer
   CRC golden checks (agentic-first).

## Honest scope

This is a **multi-week/large** effort (a reference-scale GPU translator), consistent with the ROADMAP's
"M5 is the gate." The non-graphics runtime work (which is essentially done) was the tractable part;
graphics is the reference-implementation-scale part. Biggest accelerators, in order: **(1) AGC SDK
struct headers**, (2) shadPS4's Vulkan+IR+SPIR-V spine, (3) an offscreen-Vulkan CI harness with
framebuffer-hash golden checks so every step is programmatically verified.
