# prosper

A PS5 (Prospero) → **Windows/Linux** user-space compatibility layer — *Proton for PS5*.

Not a CPU emulator: the PS5 is x86-64, so guest code runs **natively**. `prosper`
translates the operating system (FreeBSD-derived), the library ABI (Sony NID-linked
modules), and the GPU (AGC → Vulkan) underneath the unmodified game binary.

**Target title:** `PPSA24651` — *The Messenger* (Unity 2022 / IL2CPP), whose dump
lives in `../PPSA24651-app0`. Its SELF segments are unencrypted, which is what
makes this project possible without console keys.

## Status
- ✅ **M0 — Recon & tooling.** Format cracked, full HLE work-list extracted. See
  [`docs/FINDINGS.md`](docs/FINDINGS.md).
- ⏳ **M1 — Loader** (next).

See [`docs/ROADMAP.md`](docs/ROADMAP.md) for milestones and
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the design.

## Layout
```
prosper/
  docs/            architecture, roadmap, findings
  tools/
    self_dump/     SELF/ELF inspector — NID import extractor  (built, working)
  src/             (loader / hle / kernel / gpu / io — coming, per roadmap)
  CMakeLists.txt
```

## Build
```
cmake -S . -B build -G Ninja
cmake --build build
```
Or the tool directly:
```
g++ -O2 -std=c++20 tools/self_dump/self_dump.cpp -o self_dump
./self_dump ../PPSA24651-app0/eboot.bin [--symbols]
```

## Legal / scope
Interoperability & preservation research on a legally-owned title. `prosper` ships
**no** Sony code, firmware, or keys — it reimplements published library interfaces.
It only operates on **already-unencrypted** dumps; it does not defeat DRM/encryption.
