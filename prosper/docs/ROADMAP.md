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

### M1 — Loader  🟡 (core done & tested; deps/TLS/real-mmap pending)
Turn the file into a resident, relocated guest image in host memory.
- [x] Parse SELF→ELF; build VA→file map; collect segments/dynamic/symbols/relocs/imports
      (`src/self/module.{hpp,cpp}`).
- [x] Build flat image at a chosen guest base; per-segment protection records.
- [x] Apply relocations (`R_X86_64_RELATIVE`/`GLOB_DAT`/`JUMP_SLOT`). *(TLS relocs deferred to M3.)*
- [x] Bind imports to stub slots (mechanism for M2's trap stubs).
- [x] Locate entry; assert it lands in an executable segment with real code.
- [ ] Real host backing: `mmap`(`MAP_FIXED`)/`VirtualAlloc` behind the image (M2, Linux).
- [ ] Load dependent `*.prx`; build module graph; unify address space.
- [ ] TLS templates, `sce_process_param` wiring.
- **Verify (programmatic, GREEN):** `tests/test_module.cpp` — 24/24 checks against the
  real `eboot.bin`: identity, segment/import counts (612 imports / 35 libs), reloc
  counts, and spot-checked relocation results (JUMP_SLOT→stub, RELATIVE→base+addend)
  read back from the built image. Runs hermetically via `ctest` (static binary).

### M2 — First execution  🟢 (core done; readable names + real dispatch next)
- [x] Real host backing: `mmap(MAP_FIXED_NOREPLACE)` maps the relocated image at
      its guest base as executable; import stub region mapped `PROT_NONE`
      (`src/host/exec_image_linux.cpp`).
- [x] Import trap: `SIGSEGV` handler identifies the faulting stub → `lib::NID`.
- [x] Minimal bootstrap: SysV initial stack + `argc/argv` block; jump to entry.
- [x] **Guest executes**: `eboot` crt runs and traps at its **first Sony call**
      (`libc::bzQExy189ZI`). *This is the "it's alive" moment.* ✅
- [x] NID hash (SHA1 + 16-byte salt + Sony base64) + name↔NID DB; traps now print
      readable names (`src/hle/nid.{hpp,cpp}`). Validated: `memcpy`→`Q3VBxCXhUHs`,
      105/159 dictionary names resolve real eboot imports.
- [x] Real HLE **dispatch**: per-import executable stubs — implemented imports
      tail-jump to a C handler (args intact); unimplemented ones log + return 0 so the
      boot advances (`src/hle/dispatch.*`, stubs in `exec_image_linux.cpp`).
- [x] First HLE module: `src/hle/hle_libc.cpp` — libc thunks (mem/str/heap) + CRT
      no-ops, registered by NID. Boot trace now shows the real startup call order.
- [x] Stack alignment fixed (Sony crt wants entry rsp ≡ 8 mod 16); boot clears
      C++ static init.
- [x] NID name DB integrated: `known_names.txt` (idc/ps4libdoc, 42k names) auto-loaded
      into `NidDb` → 476/612 imports named in traces. (Fetch via `tools/fetch_niddb.sh`.)
- [x] libc: mem/str/heap (+`memalign`), `std::call_once` (`_Execute_once`), CRT no-ops.
- [x] libkernel: pthread mutex/cond (+attrs), `scePthreadSelf`; virtual/direct **memory**
      (`sceKernelReserveVirtualRange`, `MapNamedFlexibleMemory`, `AllocateDirectMemory`,
      `MapDirectMemory`, `Munmap`, `Mprotect`, `VirtualQuery`) backed by host mmap.
- [x] Memory model: track mappings; honor reservation alignment; `VirtualQuery` reports
      real regions/holes; `DirectMemoryQuery` + direct-memory tracking.
- [x] Time/clock sources (advancing) — broke a wait-for-time spin.
- [x] **Real multithreading**: `scePthreadCreate` runs the guest thread entry on a host
      pthread (ABI matches); join/detach/attrs; C11 `_Mtx_*`/`_Cnd_*`.
- [x] Assorted stubs: `sceSysmoduleLoadModule`, `sceKernelLoadStartModule`, UUID, sched hints.
- [x] Thread-safe fault handling (per-thread recovery point); crash-proof forked boot test.
- **Boot now runs deep**: crt → C++/`call_once` init → pthread/mutex init → heap →
  virtual+direct memory setup → time → **spawns worker threads** → reaches game
  `Il2cppUserAssemblies` imports, `libScePosix` file I/O, and locale/ctype init
  (~17 distinct unimplemented calls deep).
- [x] **Multi-module dynamic linker** (`src/loader/linker.cpp`): loads the main exe +
      dependent PRX (esp. **`Il2cppUserAssemblies` — the game's compiled C#**) into one
      address space, builds a global export table, resolves imports to real cross-module
      targets or dedup'd HLE stub slots. (3 modules, 914 imports, 11 cross-module.)
- [x] C++ runtime: `operator new`/`delete` (all variants) → host heap.
- [x] stdio: `printf`/`puts`/`snprintf`/`v*` (va_list forwarded) → host.
- [x] File I/O (`src/hle/hle_file.cpp`): stdio `FILE*` + POSIX fd + `sceKernelOpen/…`,
      with **`/app0` → dump-dir path translation** (real asset loading).
- **Now**: the game's own IL2CPP code executes; boot advances through crt → C++/threads →
  memory → stdio → file I/O → locale/ctype init (`_Getpctype`).
- [x] Dependent-module `init_array` (C++ global ctors) now run before entry — the key
      unlock that let IL2CPP's runtime initialize.
- [x] locale/ctype tables; `sync_on_address` futex (Linux `futex(2)`); C++ new/delete; stdio.
- [x] Fault backtrace (rbp-chain walk) for debugging deep crashes.
- **The game's `main()` runs and prints output** (`"Argument Count = 1 … /app0/eboot.bin"`),
  then initializes PS5 services: user (accessibility), NP/online state, **controller
  (`scePadOpen`)**, mouse, AppContent, CommonDialog.
- [x] PS5 system services (`src/hle/hle_service.cpp`): user (initial user, name, accessibility),
      NP/online (signed-out state, account), pad/mouse (open→handle, zeroed state), app content,
      dialogs — openers return handles, queries zero their output + report sane state.
- [x] pthread TLS **keys** (`pthread_key_create`/`get`/`setspecific`) → host pthread keys (IL2CPP TLS).
- [x] `boot_trace` debug tool (`tools/boot_trace/`): links all modules, boots, prints the
      unimplemented-call trace + register state + module-classified rbp backtrace on fault.
- **Now**: boots deep into the **IL2CPP runtime** (backtrace: main→eboot→Il2cpp+0x107752→…→0x13ade0).
- [ ] **Current frontier**: genuine stack corruption inside IL2CPP runtime init (fn `Il2cpp+0x13ade0`;
      its stack canary fails → `__stack_chk_fail` → `ud2`/SIGILL). Suspect a HLE data-size/ABI
      mismatch feeding a stack buffer, or missing **TLS (`%fs`)**. Next: set up guest TLS with
      `%fs`-swap trampolines at the HLE boundary (the big remaining foundational piece), and
      trace `0x13ade0`'s callees for the corrupting write.
- [ ] Then: **`libSceVideoOut` (window/swapchain)** and **`libSceAgc` → Vulkan + RDNA2 shader
      recompiler** (first frame).
- **Verify (programmatic, GREEN):** `tests/test_trap_linux.cpp` (map + identify 4
  representative imports) and `tests/test_boot_linux.cpp` (jump into real entry,
  assert it reaches an import trap). Both headless, exit-code = truth.

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
