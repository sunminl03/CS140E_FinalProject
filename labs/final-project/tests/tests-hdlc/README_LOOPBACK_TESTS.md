# HDLC Loopback Tests

There are two versions of the loopback test:

## 1. `hdlc_loopback_test.c` (Continuous Loop)

- Runs forever in a loop
- Tests continuous communication
- **Requires killing `make`** after bootloader finishes
- Use when you want to test multiple frames or continuous operation

**Workflow:**
```bash
# Terminal 1
make
# Wait for "HDLC loopback test starting..."
# Then kill with Ctrl+C or: pkill my-install

# Terminal 2
python3 tests/tests-hdlc/hdlc_laptop_test.py
```

## 2. `hdlc_single_loopback_test.c` (Single Iteration)

- Runs once and exits
- Prints "DONE!!!" so `my-install` exits automatically after receiving one frame
- **Still need to coordinate timing** - Python script should run while Pi is waiting
- Use for quick single-frame tests

**Workflow:**
```bash
# Update Makefile:
# Comment out: PROGS += tests/tests-hdlc/hdlc_loopback_test.c
# Uncomment: PROGS += tests/tests-hdlc/hdlc_single_loopback_test.c

# Terminal 1
make
# Wait for "HDLC single loopback test starting..." and "Waiting for frame..."
# Pi is now waiting for a frame, but my-install still has the port

# Terminal 2 (run while make is still running)
python3 tests/tests-hdlc/hdlc_laptop_test.py
# This will fail to open port because my-install has it

# So you still need to either:
# Option A: Kill my-install after seeing "Waiting for frame..."
# Option B: Use the continuous test and kill make (simpler!)
```

**Note:** The single test still requires killing `my-install` because `pi_echo()` holds the port open while the Pi is waiting. The advantage is that `my-install` exits automatically AFTER the frame is processed, but you still need to free the port before the Python script can run.

## Why the Difference?

- `pi_echo()` (used by `my-install`) exits when it sees "DONE!!!" from the Pi
- The continuous test runs forever, so it never prints "DONE!!!"
- The single test prints "DONE!!!" after one iteration, so `my-install` exits naturally

Both tests verify the same thing: HDLC framing works correctly in both directions.
