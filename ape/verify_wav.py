#!/usr/bin/env python3
"""Check a recorded WAV: 1 channel, 16 kHz, 16-bit, and a duration
inside [min, max] seconds (defaults: a ~3 s take, 2.8-3.3)."""
import sys
import wave

path = sys.argv[1] if len(sys.argv) > 1 else "voica-out.wav"
lo = float(sys.argv[2]) if len(sys.argv) > 2 else 2.8
hi = float(sys.argv[3]) if len(sys.argv) > 3 else 3.3
w = wave.open(path)
channels, rate, width = w.getnchannels(), w.getframerate(), w.getsampwidth()
seconds = w.getnframes() / rate
print(f"{path}: channels={channels} rate={rate} sampwidth={width} seconds={seconds:.2f}")
assert channels == 1, "expected mono"
assert rate == 16000, "expected 16 kHz"
assert width == 2, "expected 16-bit"
assert lo <= seconds <= hi, f"expected between {lo} and {hi} seconds"
print("WAV OK")
