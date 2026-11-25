#!/usr/bin/env python3
"""Check a recorded WAV: 1 channel, 16 kHz, 16-bit, about 3 seconds."""
import sys
import wave

path = sys.argv[1] if len(sys.argv) > 1 else "voica-out.wav"
w = wave.open(path)
channels, rate, width = w.getnchannels(), w.getframerate(), w.getsampwidth()
seconds = w.getnframes() / rate
print(f"{path}: channels={channels} rate={rate} sampwidth={width} seconds={seconds:.2f}")
assert channels == 1, "expected mono"
assert rate == 16000, "expected 16 kHz"
assert width == 2, "expected 16-bit"
assert 2.8 <= seconds <= 3.3, "expected ~3 seconds"
print("WAV OK")
