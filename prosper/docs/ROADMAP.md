# Roadmap — `prosper`

Honest framing: a full PS5→PC layer that runs a commercial Unity title to a
playable state is a **multi-year, reference-implementation-scale effort** (cf.
shadPS4 for PS4). It is *not* infeasible for this title — the code is unencrypted
and x86-64 — but the GPU translation + shader recompiler alone are enormous.

We build it as a sequence of **independently verifiable milestones**. Each one
produces something concrete you can run and check.

---

### M0 — Recon & tooling ✅ (done)
- [x] Identify format (unencrypted SELF, x86-64, FreeBSD ABI).
- [x] `self_dump`: SELF/ELF parser, VA mapping, dynamic tags, NID import extraction.
- [x] Full HLE work-list: every imported library + function count (see FINDINGS.md).

### M1 — Loader
Turn the file into a resident, relocated guest image in host memory.
- [ ] Reserve guest VA region; map `PT_LOAD` segments with correct protections.
- [ ] Apply relocations (`R_X86_64_RELATIVE`/`GLOB_DAT`/`JUMP_SLOT`/TLS).
- [ ] Load dependent `*.prx`; build module graph; unify address space.
- [ ] Locate entry, `sce_process_param`, TLS templates.
- **Verify:** print the guest memory map; disassemble bytes at `entry` and confirm
  a sane function prologue (via `objdump`/capstone).

### M2 — HLE stub framework + first execution
- [ ] NID hash (SHA1+salt) + name↔NID database for readable logs.
- [ ] Import resolver binds every undefined symbol to a generated logging-trap stub.
- [ ] Minimal host bootstrap: stack, TLS, thread 0; jump to entry.
- **Verify:** the guest **starts executing** and stops at the first unimplemented
  Sony call, logging its real name. This is the "it's alive" moment.

### M3 — Kernel + libc/posix (reach Unity init)
- [ ] Memory: flexible/direct memory, `mmap`/`VirtualAlloc`, protections.
- [ ] Threads, mutex/cond/sema/event-flags, `sync_on_address`.
- [ ] File I/O with `/app0` → dump dir; timers, clocks, `sceKernelRtc`.
- [ ] `libc`/`libScePosix`/`libSceLibcInternal` — thunk to host libc where safe.
- **Verify:** guest advances through Unity/IL2CPP runtime init, loads assets,
  and calls into `libSceVideoOut`/`libSceAgc` (i.e., it *wants to draw*).

### M4 — Window + VideoOut + AGC capture
- [ ] `libSceVideoOut` → SDL3 window + Vulkan swapchain; flip/vsync.
- [ ] `libSceAgc`/`AgcDriver` → capture command-buffer submissions; log/parse packets.
- **Verify:** a window opens; we can dump the guest's per-frame GPU command stream.

### M5 — GPU translation + shader recompiler (the big one)
- [ ] Command/packet → Vulkan pipeline + draw translation; resource/descriptor mgmt.
- [ ] Shader recompiler: GPU ISA decode → IR → SPIR-V; resource binding model.
- **Verify:** **first correctly rendered frame** (title/splash).

### M6 — Input + Audio
- [ ] `libScePad` → SDL gamepad. `libSceAudioOut(2)` → host audio.
- [ ] `libSceAjm` (ATRAC9/AAC decode); `libSceAvPlayer` (cutscenes) via ffmpeg.
- **Verify:** menu navigable with a controller; music/SFX play.

### M7 — Services & polish (playable)
- [ ] `libSceSaveData` → host files; `libSceSystemService`/`UserService`.
- [ ] `libSceNp*` stubs (single-player: trophies/presence no-op or local).
- [ ] Dialogs (`MsgDialog`, `Ime`, `CommonDialog`).
- **Verify:** boot → menu → gameplay; save/load works.

---

## Cross-cutting
- **ABI shim** (Windows host only): SysV⇄MS-x64 trampolines at every call boundary.
- **Tracing**: every HLE call logged behind a flag; per-module enable.
- **Test harness**: golden memory-map & call-trace snapshots to catch regressions.

## Reality checkpoints
- After **M2** we know exactly the call order the game needs — re-prioritize M3+.
- **M5 is the gate.** If the shader recompiler proves intractable for AGC, options
  are: (a) target the simpler subset this 2D-ish game uses, (b) reuse an existing
  RDNA recompiler. The Messenger is 2D/sprite-based → likely a *small* shader set,
  which is a genuinely favorable draw for a first title.
