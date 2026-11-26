#!/usr/bin/env python3
"""CI helper: run the APE in --stream mode, then stop it the way a
terminal's Ctrl+C does — SIGINT to the whole process GROUP.

Signalling only the launcher's PID does not work: the launcher waits
inside system(), which shields it from SIGINT, and the host doing the
recording is a grandchild. A real Ctrl+C never has this problem (the
kernel signals the foreground group), so the test does the same.

Usage: stream_test.py <seconds-to-record> <command...>
Exits with the command's exit code (0 = stopped cleanly)."""
import os
import shlex
import signal
import subprocess
import sys
import time

wait_s = float(sys.argv[1])
# Run through a shell with exec: an APE file is not directly execve-able
# on every system (its shell-script bootstrap is what starts it), and
# exec keeps one process as the group leader for the signal below.
cmd = "exec " + shlex.join(sys.argv[2:])
p = subprocess.Popen(cmd, shell=True, start_new_session=True)
time.sleep(wait_s)
os.killpg(p.pid, signal.SIGINT)
try:
    rc = p.wait(timeout=30)
except subprocess.TimeoutExpired:
    p.kill()
    print("stream test: process did not stop on SIGINT", flush=True)
    sys.exit(1)
print(f"stream test: exited {rc} after SIGINT", flush=True)
# Success = it stopped promptly. The exit may be 0 (host caught the
# signal and shut down cleanly) or -SIGINT (the launcher itself died
# by the signal — normal Ctrl+C semantics for a CLI). Both are fine;
# the WAV check that follows is the real assertion.
sys.exit(0 if rc in (0, -signal.SIGINT) else 1)
