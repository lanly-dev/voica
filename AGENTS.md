# AGENTS.md — Voica

Tiny WebAssembly runtime with a microphone: portable `.wasm` apps,
per-OS hosts (WAMR interpreter + a miniaudio `record` bridge).

## Layout

- `host/main.c` — WAMR embedding, native registration (module `"voica"`).
- `host/bridge.c` / `bridge.h` — the only native surface: capture to WAV
  via miniaudio. `host/miniaudio.h` is vendored 0.11.22.
- `app/recorder.c` — freestanding demo app (no libc, no WASI).
- `build.sh` / `build.bat` — fetch pinned WAMR (WAMR-2.4.6) into
  `third_party/`, build host + app into `dist/`.

## Rules

- The bridge is the contract: apps import `voica.record` / `voica.log`
  and nothing else. New capabilities go through the bridge deliberately,
  never as ad-hoc imports.
- Apps stay freestanding — no libc, no WASI. If an app needs a library,
  that's a design discussion, not a silent dependency.
- WAMR stays pinned and fetched, not vendored; bump the tag in both
  build scripts together.
- No absolute local paths in anything committed — repo-relative only.
- Verify by running the loop: build, run `dist/voica dist/recorder.wasm`
  (from `dist/`), and check the WAV's format and duration.
