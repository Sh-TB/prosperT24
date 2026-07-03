# prosper

**A user-space PlayStation 5 → PC compatibility layer.** Think *Proton/Wine, but for PS5*: prosper
runs PS5 (Prospero) game binaries on Linux (primary) and Windows (bonus) by translating the
console's OS, ABI, and (eventually) GPU calls to the host — **not** by emulating a CPU.

> ⚠️ **Early, experimental research project.** It does not run games to playable state yet. See
> [Status](#status) for exactly how far it gets today.

## Why no CPU emulation?

The PS5 is an x86-64 (AMD Zen 2) machine, so guest game code runs **natively** on any modern PC.
prosper is therefore shaped like Wine, not like an emulator: the hard work is

- **Loading** Sony's `SELF`/`ELF` module format and linking multiple modules into one address space,
- **HLE** (high-level emulation) of the PS5 system libraries (`libkernel`, `libc`, `libSceGnmDriver`/
  `libSceAgc`, `libScePad`, …) by thunking to the host's real facilities (libc, pthreads, `mmap`,
  `futex`, …),
- **ABI translation** where the guest's FreeBSD/SysV conventions differ from the host, and
- **Graphics translation** (the eventual end goal): PS5 **AGC** → Vulkan, including an RDNA2 shader
  recompiler. This is the largest remaining piece and is intentionally *not* the current focus.

Because the PS5 and Steam Deck / AMD Linux machines share the same RDNA2 GPU family, Linux is the
natural primary target and the long-term dream is running on that hardware.

## Status

The current bring-up target is a real Unity/IL2CPP title. Starting from its (user-supplied,
**unencrypted-segment**) module set, prosper today:

- ✅ Parses `SELF`/`ELF`, builds a relocatable image, resolves Sony **NID**-hashed imports.
- ✅ Links the game's modules together with a global export table + per-import HLE stubs.
- ✅ Runs the Sony CRT, C++ global constructors, and `il2cpp` startup.
- ✅ Implements enough `libkernel`/`libc` for real memory management (virtual + direct memory),
  threads (pthreads, TLS, mutexes/conds, event flags, semaphores, `sync_on_address` futex),
  time, file I/O (with `/app0` path translation), and locale/ctype.
- ✅ Loads the game's C# metadata and **spins up the full IL2CPP GC + worker thread pool.**
- 🚧 **Currently blocked on a threading deadlock during `il2cpp_init`** — precisely diagnosed to a
  counting semaphore that never gets posted while the whole thread pool is parked. This is being
  fixed *correctly* (no forced wakeups / fakes).
- ⬜ Graphics (VideoOut window, AGC→Vulkan, shader recompiler) — future work.

Development is **agentic-first**: correctness is verified programmatically (unit tests, structured
logs, purpose-built debug tooling like `boot_trace` and `PROSPER_SYNCLOG`) rather than by hand.

## Legal / ethical

- **No game files, keys, or copyrighted Sony code are included** in this repository, and none ever
  will be. You must supply your own legally-obtained dump.
- prosper only works with **unencrypted** module segments; it contains **no** console decryption
  keys and performs no circumvention of Sony's cryptography.
- This is an independent interoperability / research project, not affiliated with or endorsed by
  Sony Interactive Entertainment.

## Building (Linux)

Requires a C++20 compiler, CMake, and Ninja.

```sh
cd prosper
cmake -G Ninja -B build-linux
cmake --build build-linux
ctest --test-dir build-linux
```

A static-linked Windows (mingw) build works for the host-agnostic parts as well.

## Repository layout

```
prosper/
  src/self/       SELF/ELF parsing → relocatable module image
  src/loader/     multi-module linker + global export table
  src/hle/        HLE of Sony libraries (libc, libkernel, services), NID hashing
  src/host/       host execution: image mapping, stubs, fault handling (Linux)
  tools/          debug tooling (boot_trace, self_dump, …)
  tests/          unit + boot tests (run under ctest)
  docs/           ROADMAP.md, FINDINGS.md
```

## License

See [LICENSE](LICENSE) if present. Game content and Sony SDK symbols are the property of their
respective owners and are not distributed here.
