/*
 * Voica demo app — portable WebAssembly. This one .wasm file runs
 * unchanged on every Voica host (Windows, macOS, Linux); the host
 * supplies the microphone through the "voica" import module.
 *
 * Freestanding: no libc, no WASI. Build:
 *   clang --target=wasm32 -nostdlib -O2 -Wl,--no-entry -Wl,--export=run \
 *         -o recorder.wasm recorder.c
 */

__attribute__((import_module("voica"), import_name("record")))
int voica_record(const char *path, int path_len, int seconds);

__attribute__((import_module("voica"), import_name("log")))
void voica_log(const char *msg, int msg_len);

static const char kMsgStart[] = "recording (default 3 seconds to voica-out.wav; host options can override both)";
static const char kMsgOk[] = "done — recording written";
static const char kMsgFail[] = "record failed";
static const char kOutPath[] = "voica-out.wav";

__attribute__((export_name("run")))
void run(void) {
    voica_log(kMsgStart, (int)(sizeof(kMsgStart) - 1));
    int rc = voica_record(kOutPath, (int)(sizeof(kOutPath) - 1), 3);
    if (rc == 0) {
        voica_log(kMsgOk, (int)(sizeof(kMsgOk) - 1));
    } else {
        voica_log(kMsgFail, (int)(sizeof(kMsgFail) - 1));
    }
}
