# Roadmap — `prosper`

Honest framing: a full PS5→PC layer that runs a commercial Unity title to a
playable state is a **multi-year, reference-implementation-scale effort** (cf.
shadPS4 for PS4). It is *not* infeasible for this title — the code is unencrypted
and x86-64 — but the GPU translation + shader recompiler alone are enormous.

We build it as a sequence of **independently verifiable milestones**. Each one
produces something concrete you can run and check.

## Verification philosophy — agentic-first (non-negotiable)

This project is built to be completed by **AI agents with no human in the loop**.
Therefore **no milestone is "done" on human observation.** Every `Verify:` step
below is a **programmatic, headless, self-checking** gate:
- automated tests + assertions (exit code = truth),
- structured, greppable logs (per-module call tracing behind flags),
- deterministic golden snapshots (memory maps, call traces, **framebuffer pixel
  hashes**) checked by CI, not by eye,
- debugging tooling shipped alongside each feature (map dumpers, packet loggers,
  disassembly helpers, headless render + hash compare).
"A window opens" is never a criterion; "test asserts swapchain created, N frames
presented, framebuffer CRC == golden" is. Each change adds the check that proves it.

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
- **Verify (programmatic):** headless run asserts swapchain created + ≥N flips
  presented; per-frame GPU command stream dumped to file and asserted to contain
  the expected packet opcodes. No human viewing.

### M5 — GPU translation + shader recompiler (the big one)
- [ ] Command/packet → Vulkan pipeline + draw translation; resource/descriptor mgmt.
- [ ] Shader recompiler: GPU ISA decode → IR → SPIR-V; resource binding model.
- **Verify (programmatic):** headless render to offscreen target; **framebuffer
  pixel-hash matches a golden snapshot** (captured once, then regression-gated).
  Shader recompiler has unit tests: ISA fixture → expected SPIR-V/behavior.

### M6 — Input + Audio
- [ ] `libScePad` → SDL gamepad. `libSceAudioOut(2)` → host audio.
- [ ] `libSceAjm` (ATRAC9/AAC decode); `libSceAvPlayer` (cutscenes) via ffmpeg.
- **Verify (programmatic):** inject **synthetic pad input** → assert guest state
  transitions in the call trace; assert audio-out ringbuffer receives non-silent
  PCM (RMS > threshold); `libSceAjm` decode of a fixture matches a reference hash.

### M7 — Services & polish (playable)
- [ ] `libSceSaveData` → host files; `libSceSystemService`/`UserService`.
- [ ] `libSceNp*` stubs (single-player: trophies/presence no-op or local).
- [ ] Dialogs (`MsgDialog`, `Ime`, `CommonDialog`).
- **Verify (programmatic):** a **scripted input sequence** drives boot→menu→gameplay
  with no human; save then reload asserts state round-trips byte-for-byte; the full
  boot call-trace is diffed against a golden trace.

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
