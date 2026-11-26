#ifndef VOICA_BRIDGE_H
#define VOICA_BRIDGE_H

/* The host's one hardware bridge: record `seconds` of audio from the
   default capture device to a 16 kHz mono 16-bit WAV file at
   `outputPath`. Returns 0 on success, non-zero on failure.
   Implemented on miniaudio (WASAPI / CoreAudio / ALSA-Pulse). */
int voica_bridge_record(const char *outputPath, int seconds);

/* Host command-line overrides, set by main() before the app runs. They
   let the person running the host — not the app — choose the take:
   - seconds override: record this many seconds instead of the app's
     request (-1 = no override, the default).
   - output override: write to this path instead of the app's path
     (NULL = no override, the default). */
void voica_bridge_set_seconds_override(int seconds);
void voica_bridge_set_output_override(const char *path);

#endif
