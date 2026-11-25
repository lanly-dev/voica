/*
 * Voica host — a tiny WebAssembly runtime with a microphone.
 *
 * The JVM pattern in miniature: apps are portable .wasm files that run
 * unchanged on every OS; the host is the per-OS part (WAMR interpreter +
 * the miniaudio bridge) that gives them the one hardware capability WASI
 * never standardized — recording audio.
 *
 * Usage: voica <app.wasm>
 * The app must export `run()`. It may import from module "voica":
 *   record(path_ptr, path_len, seconds) -> i32   (0 = success)
 *   log(msg_ptr, msg_len)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wasm_export.h"
#include "bridge.h"

/* ---- native bridge functions (module "voica") ---- */

static int32_t native_record(wasm_exec_env_t exec_env, const char *path, uint32_t path_len, int32_t seconds) {
    (void)exec_env;
    /* The app's string is not NUL-terminated — copy it out defensively. */
    char *buf = (char *)malloc((size_t)path_len + 1);
    if (buf == NULL) return 1;
    memcpy(buf, path, path_len);
    buf[path_len] = '\0';
    int rc = voica_bridge_record(buf, (int)seconds);
    free(buf);
    return rc;
}

static void native_log(wasm_exec_env_t exec_env, const char *msg, uint32_t msg_len) {
    (void)exec_env;
    printf("[app] %.*s\n", (int)msg_len, msg);
    fflush(stdout);
}

static NativeSymbol voica_natives[] = {
    { "record", (void *)native_record, "(*~i)i", NULL },
    { "log", (void *)native_log, "(*~)", NULL },
};

/* ---- host ---- */

static unsigned char *read_file(const char *path, long *out_size) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = (unsigned char *)malloc((size_t)size);
    if (buf == NULL) {
        fclose(f);
        return NULL;
    }
    if (size > 0 && fread(buf, 1, (size_t)size, f) != (size_t)size) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *out_size = size;
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: voica <app.wasm>\n");
        return 2;
    }

    long wasm_size = 0;
    unsigned char *wasm_buf = read_file(argv[1], &wasm_size);
    if (wasm_buf == NULL) {
        fprintf(stderr, "voica: cannot read %s\n", argv[1]);
        return 1;
    }

    if (!wasm_runtime_init()) {
        fprintf(stderr, "voica: runtime init failed\n");
        free(wasm_buf);
        return 1;
    }
    if (!wasm_runtime_register_natives("voica", voica_natives,
                                       sizeof(voica_natives) / sizeof(voica_natives[0]))) {
        fprintf(stderr, "voica: failed to register native bridge\n");
        wasm_runtime_destroy();
        free(wasm_buf);
        return 1;
    }

    int exit_code = 1;
    char error_buf[256];
    wasm_module_t module = wasm_runtime_load(wasm_buf, (uint32_t)wasm_size, error_buf, sizeof(error_buf));
    if (module == NULL) {
        fprintf(stderr, "voica: load failed: %s\n", error_buf);
        goto done;
    }

    wasm_module_inst_t module_inst = wasm_runtime_instantiate(module, 64 * 1024, 16 * 1024,
                                                              error_buf, sizeof(error_buf));
    if (module_inst == NULL) {
        fprintf(stderr, "voica: instantiate failed: %s\n", error_buf);
        goto unload;
    }

    wasm_function_inst_t run_func = wasm_runtime_lookup_function(module_inst, "run");
    if (run_func == NULL) {
        fprintf(stderr, "voica: app does not export run()\n");
        goto deinstantiate;
    }

    wasm_exec_env_t exec_env = wasm_runtime_create_exec_env(module_inst, 64 * 1024);
    if (exec_env == NULL) {
        fprintf(stderr, "voica: exec env creation failed\n");
        goto deinstantiate;
    }

    if (wasm_runtime_call_wasm(exec_env, run_func, 0, NULL)) {
        exit_code = 0;
    } else {
        fprintf(stderr, "voica: app exception: %s\n", wasm_runtime_get_exception(module_inst));
    }
    wasm_runtime_destroy_exec_env(exec_env);

deinstantiate:
    wasm_runtime_deinstantiate(module_inst);
unload:
    wasm_runtime_unload(module);
done:
    wasm_runtime_destroy();
    free(wasm_buf);
    return exit_code;
}
