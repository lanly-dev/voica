/*
 * The Voica host's audio bridge — the per-OS native code behind the one
 * function apps get: voica.record(). Built on miniaudio (vendored single
 * header): WASAPI on Windows, CoreAudio on macOS, ALSA/PulseAudio on
 * Linux. The WAV file is written by hand (fixed 44-byte PCM header,
 * sizes patched at close): in miniaudio 0.11.22 the WAV encoder backend
 * lives under the decoding gate, so MA_NO_DECODING silently removes it —
 * writing the one fixed format directly keeps every size trim on.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/* Capture only — keep everything else out of the binary. */
#define MA_NO_DECODING
#define MA_NO_NODE_GRAPH
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_GENERATION
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "bridge.h"

#define SAMPLE_RATE 16000
#define CHANNELS 1
#define BITS_PER_SAMPLE 16
#define BLOCK_ALIGN (CHANNELS * BITS_PER_SAMPLE / 8)

typedef struct {
    FILE *pFile;
    /* Written by the capture callback, read by the record loop: the loop
     * stops on this count, not on wall-clock time (see below). */
    volatile ma_uint64 framesWritten;
    /* Exact frame budget for this take (seconds * SAMPLE_RATE). 0 means
     * no budget — the callback writes everything it is given. */
    ma_uint64 targetFrames;
    /* Stream mode only: framesWritten at the last header patch. The
     * header is re-patched every half second of audio so the file on
     * disk is a valid WAV at any moment, even if the process is killed
     * instead of stopped cleanly. Only touched in the callback. */
    ma_uint64 framesAtLastPatch;
    int stream;
} wav_writer;

/* Host command-line overrides (see bridge.h). */
static int g_seconds_override = -1;
static const char *g_output_override = NULL;
static int g_stream = 0;

void voica_bridge_set_seconds_override(int seconds) { g_seconds_override = seconds; }
void voica_bridge_set_output_override(const char *path) { g_output_override = path; }
void voica_bridge_set_stream(int enabled) { g_stream = enabled; }

/* Set by SIGINT/SIGTERM while streaming; the record loop polls it. */
static volatile sig_atomic_t g_stop_requested = 0;

static void on_stop_signal(int sig) {
    (void)sig;
    g_stop_requested = 1;
}

static void write_u32le(FILE *pFile, ma_uint32 value) {
    unsigned char bytes[4];
    bytes[0] = (unsigned char)(value & 0xFF);
    bytes[1] = (unsigned char)((value >> 8) & 0xFF);
    bytes[2] = (unsigned char)((value >> 16) & 0xFF);
    bytes[3] = (unsigned char)((value >> 24) & 0xFF);
    fwrite(bytes, 1, 4, pFile);
}

static void write_u16le(FILE *pFile, ma_uint16 value) {
    unsigned char bytes[2];
    bytes[0] = (unsigned char)(value & 0xFF);
    bytes[1] = (unsigned char)((value >> 8) & 0xFF);
    fwrite(bytes, 1, 2, pFile);
}

static void wav_write_header(FILE *pFile) {
    fwrite("RIFF", 1, 4, pFile);
    write_u32le(pFile, 0); /* RIFF chunk size, patched at close */
    fwrite("WAVE", 1, 4, pFile);
    fwrite("fmt ", 1, 4, pFile);
    write_u32le(pFile, 16);
    write_u16le(pFile, 1); /* PCM */
    write_u16le(pFile, CHANNELS);
    write_u32le(pFile, SAMPLE_RATE);
    write_u32le(pFile, SAMPLE_RATE * BLOCK_ALIGN);
    write_u16le(pFile, BLOCK_ALIGN);
    write_u16le(pFile, BITS_PER_SAMPLE);
    fwrite("data", 1, 4, pFile);
    write_u32le(pFile, 0); /* data size, patched at close */
}

static void wav_patch_sizes(wav_writer *pWriter) {
    ma_uint32 dataSize = (ma_uint32)(pWriter->framesWritten * BLOCK_ALIGN);
    fseek(pWriter->pFile, 4, SEEK_SET);
    write_u32le(pWriter->pFile, 36 + dataSize);
    fseek(pWriter->pFile, 40, SEEK_SET);
    write_u32le(pWriter->pFile, dataSize);
    /* Leave the file positioned at the end so capture can keep
     * appending after a mid-take patch (stream mode). */
    fseek(pWriter->pFile, 0, SEEK_END);
}

static void sleep_ms(unsigned int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

static void on_data(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount) {
    wav_writer *pWriter = (wav_writer *)pDevice->pUserData;
    (void)pOutput;
    if (pWriter != NULL && pInput != NULL) {
        /* Clamp the write to the remaining budget so the file holds
         * exactly targetFrames, never a partial callback chunk more. */
        ma_uint64 framesToWrite = frameCount;
        if (pWriter->targetFrames > 0 && pWriter->framesWritten < pWriter->targetFrames) {
            ma_uint64 remaining = pWriter->targetFrames - pWriter->framesWritten;
            if (framesToWrite > remaining) {
                framesToWrite = remaining;
            }
        } else if (pWriter->targetFrames > 0) {
            framesToWrite = 0;
        }
        if (framesToWrite > 0) {
            pWriter->framesWritten += fwrite(pInput, BLOCK_ALIGN, (size_t)framesToWrite, pWriter->pFile);
        }
        /* Stream mode: keep the on-disk header honest as we go (see
         * framesAtLastPatch). All file writes happen in this callback,
         * so patching here cannot race another writer. */
        if (pWriter->stream &&
            pWriter->framesWritten - pWriter->framesAtLastPatch >= SAMPLE_RATE / 2) {
            wav_patch_sizes(pWriter);
            fflush(pWriter->pFile);
            pWriter->framesAtLastPatch = pWriter->framesWritten;
        }
    }
}

int voica_bridge_record(const char *outputPath, int seconds) {
    /* Host overrides win over the app's request (see bridge.h). */
    if (g_output_override != NULL) outputPath = g_output_override;
    if (g_seconds_override > 0) seconds = g_seconds_override;
    if (seconds < 1) seconds = 1;
    if (seconds > 3600) seconds = 3600;

    wav_writer writer;
    writer.framesWritten = 0;
    writer.framesAtLastPatch = 0;
    writer.stream = g_stream;
    /* Stream mode has no frame budget: it records until stopped. */
    writer.targetFrames = g_stream ? 0 : (ma_uint64)seconds * SAMPLE_RATE;
    writer.pFile = fopen(outputPath, "wb");
    if (writer.pFile == NULL) {
        fprintf(stderr, "voica: cannot open output file: %s\n", outputPath);
        return 1;
    }
    wav_write_header(writer.pFile);

    ma_device device;
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.format = ma_format_s16;
    deviceConfig.capture.channels = CHANNELS;
    deviceConfig.sampleRate = SAMPLE_RATE;
    deviceConfig.dataCallback = on_data;
    deviceConfig.pUserData = &writer;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        fprintf(stderr, "voica: no capture device available\n");
        fclose(writer.pFile);
        remove(outputPath);
        return 1;
    }
    if (ma_device_start(&device) != MA_SUCCESS) {
        fprintf(stderr, "voica: failed to start capture device\n");
        ma_device_uninit(&device);
        fclose(writer.pFile);
        remove(outputPath);
        return 1;
    }

    if (g_stream) {
        /* Stream: record until the user stops us (Ctrl+C / SIGTERM).
         * The callback keeps the header patched as audio lands, so the
         * file is valid even if we're killed before this loop exits.
         * A 24 h cap is the safety net against a forgotten recording. */
        g_stop_requested = 0;
        void (*prev_int)(int) = signal(SIGINT, on_stop_signal);
        void (*prev_term)(int) = signal(SIGTERM, on_stop_signal);
        int waitedMs = 0;
        while (!g_stop_requested && waitedMs < 24 * 3600 * 1000) {
            sleep_ms(50);
            waitedMs += 50;
        }
        signal(SIGINT, prev_int);
        signal(SIGTERM, prev_term);
    } else {
        /* Stop on FRAMES, not on wall-clock time. A capture device is
         * free to deliver frames faster or slower than real time — the
         * mic-less macOS runners deliver at ~24 kHz while the header
         * promises 16 kHz, so the old sleep-for-N-seconds loop wrote
         * 4.74 s / 4.50 s files for a 3 s take. Waiting for exactly
         * seconds * 16000 frames makes the file exactly `seconds` long
         * whatever rate the device runs at. The wall-clock cap (3x the
         * take, plus a 2 s grace) is only a safety net so a stalled
         * device can never hang the host forever; if it trips, the
         * file is short but valid. */
        int waitedMs = 0;
        int timeoutMs = seconds * 3000 + 2000;
        while (writer.framesWritten < writer.targetFrames && waitedMs < timeoutMs) {
            sleep_ms(10);
            waitedMs += 10;
        }
    }

    ma_device_stop(&device);
    ma_device_uninit(&device);
    wav_patch_sizes(&writer);
    fclose(writer.pFile);
    return 0;
}
