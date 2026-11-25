#ifndef VOICA_BRIDGE_H
#define VOICA_BRIDGE_H

/* The host's one hardware bridge: record `seconds` of audio from the
   default capture device to a 16 kHz mono 16-bit WAV file at
   `outputPath`. Returns 0 on success, non-zero on failure.
   Implemented on miniaudio (WASAPI / CoreAudio / ALSA-Pulse). */
int voica_bridge_record(const char *outputPath, int seconds);

#endif
