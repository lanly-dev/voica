# Voica

A tiny WebAssembly runtime with a microphone — the JVM pattern in
miniature, built to answer one question: *can you write a recorder once
and run it on every OS?*

Apps are portable `.wasm` files that run unchanged anywhere. The **host**
is the per-OS part: a WAMR interpreter plus one native bridge function,
`voica.record()`, implemented on miniaudio (WASAPI on Windows, CoreAudio
on macOS, ALSA/PulseAudio on Linux). That bridge is the piece WASI never
standardized — which is why this runtime exists.

## Layout

- `host/` — the runtime (C, CMake). `main.c` embeds WAMR and registers
  the bridge; `bridge.c` is the miniaudio capture code; `miniaudio.h`
  is vendored (0.11.22, public domain / MIT-0).
- `app/` — demo apps. `recorder.c` is freestanding C compiled to
  WebAssembly: it logs, records 3 seconds to `voica-out.wav`, logs done.
- `third_party/` — WAMR source, fetched by the build at a pinned tag
  (WAMR-2.4.6), not vendored.

## The app contract

Apps export `run()`. They may import from module `"voica"`:

| Import | Signature | Meaning |
| --- | --- | --- |
| `record` | `(path_ptr, path_len, seconds) -> i32` | Record to a 16 kHz mono 16-bit WAV at `path`; 0 = success |
| `log` | `(msg_ptr, msg_len)` | Print a message through the host |

Apps are freestanding: no libc, no WASI. Everything an app can do, it
does through the bridge — that's the whole design.

## Build & run

Prerequisites: cmake, a C compiler, and clang **with lld** (the app is
compiled with `clang --target=wasm32`; on Debian/Ubuntu that's the
`clang` and `lld` packages).

```sh
./build.sh            # Windows: build.bat
cd dist
./voica recorder.wasm
```

Releases are built by the **Release** workflow (`.github/workflows/`):
push a `v*` tag or run it from the Actions tab, and it publishes a
GitHub Release with `voica-linux-x64`, `voica-win32-x64.exe`,
`voica-darwin-universal` (both Mac archs lipo-merged), and the one
`recorder.wasm` that runs on all of them.

## Measured sizes (Linux x64, first build)

- Host: **565,896 bytes** stripped (~640 KB unstripped)
- Demo app (`recorder.wasm`): **480 bytes**
- The same `recorder.wasm` runs on every host build — that's the point.

## Honest notes

- The per-OS part did not disappear; it moved into the host, where it
  is paid once per platform instead of once per app. One portable app,
  three small hosts.
- The sandbox this was first built in has no microphone: its Linux
  audio stack falls back to miniaudio's null device, so the first
  end-to-end proof recorded 3.00 s of valid silence (16 kHz mono s16,
  exactly on duration). First run on a real machine is the real-mic
  calibration.
- WAMR is configured interpreter-only (no AOT/JIT, no libc/WASI) — the
  smallest honest configuration for this job.
